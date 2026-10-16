#ifdef FLUX_DELTA_BUILD
#include "core/delta_ui.h"
#endif
#include "core/bus_HAL.h"
#include "core/powerSave.h"
#include "core/settings.h"
#include "core/led_control.h"
#include "modules/ble/ble_common.h"
#include <WiFi.h>
#include "flux_battery_filter.h"
#include <atomic>
#include <bq27220.h>
#include <globals.h>
#include <interface.h>

// Rotary encoder
#include <rotary_decoder.h>
#ifdef SAKURA_BUILD
#include <delta/button_input.h>
#include "core/input_diagnostics.h"
#include "core/mykeyboard.h"
#endif
extern RotaryDecoder *encoder;
RotaryDecoder *encoder = nullptr;
void pollEncoder(void) { encoder->poll(); }

// Battery libs
#if defined(T_EMBED_1101)
// Power handler for battery detection
#include <Wire.h>
// Charger chip
#define XPOWERS_CHIP_BQ25896
#include <XPowersLib.h>
#include <esp32-hal-dac.h>
XPowersPPM PPM;
#elif defined(T_EMBED)

#endif

#ifdef USE_BQ27220_VIA_I2C
#define BATTERY_DESIGN_CAPACITY 1300
#include <bq27220.h>
BQ27220 bq;
static bool readPowerRegister(uint8_t address, uint8_t reg, uint8_t *data, uint8_t count);
#endif

#include "core/i2c_finder.h"
#include "modules/rf/rf_utils.h"
#include <Adafruit_PN532.h>

/***************************************************************************************
** Function name: _setup_gpio()
** Description:   initial setup for the device
***************************************************************************************/
void _setup_gpio() {
    // Keep reset/deep-wake GRAM invisible until the first cleared frame in begin_tft().
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, LOW);
    pinMode(PIN_POWER_ON, OUTPUT);
    digitalWrite(PIN_POWER_ON, HIGH);
    pinMode(SEL_BTN, INPUT);
#ifdef T_EMBED_1101
    // T-Embed CC1101 has a antenna circuit optimized to each frequency band, controlled by SW0 and SW1
    // Set antenna frequency settings
    pinMode(CC1101_SW1_PIN, OUTPUT);
    pinMode(CC1101_SW0_PIN, OUTPUT);

    // Chip Select CC1101, SD and TFT to HIGH State to fix SD initialization
    pinMode(CC1101_SS_PIN, OUTPUT);
    digitalWrite(CC1101_SS_PIN, HIGH);
    pinMode(TFT_CS, OUTPUT);
    digitalWrite(TFT_CS, HIGH);
    pinMode(SDCARD_CS, OUTPUT);
    digitalWrite(SDCARD_CS, HIGH);
    pinMode(NRF24_SS_PIN, OUTPUT); // NRF24 on Plus
    digitalWrite(NRF24_SS_PIN, HIGH);

    pinMode(NRF24_CE_PIN, OUTPUT); // put nRF24 in standby
    digitalWrite(NRF24_CE_PIN, LOW);

    // Power chip pin
    pinMode(PIN_POWER_ON, OUTPUT);
    digitalWrite(PIN_POWER_ON, HIGH); // Power on CC1101 and LED
    bool pmu_ret = false;
    setSysI2CBus(&Wire); // PPM/bq27220 live on the default Wire object (GROVE == sys_i2c on this variant)
    Wire.begin(SYS_I2C_SDA, SYS_I2C_SCL);
    pmu_ret = PPM.init(Wire, SYS_I2C_SDA, SYS_I2C_SCL, BQ25896_SLAVE_ADDRESS);
    if (pmu_ret) {
        // https://github.com/Xinyuan-LilyGO/T-Embed-CC1101/blob/3e6df69af51befdbd5c96761aca28b9a784413eb/examples/factory_test/factory_test.ino#L399-L425

        PPM.resetDefault();
        PPM.setChargeTargetVoltage(4208);
        PPM.enableMeasure(PowersBQ25896::CONTINUOUS);
    }
    uint8_t capacity[2] = {};
    if (readPowerRegister(BQ27220_I2C_ADDRESS, BQ27220_COMMAND_DESIGN_CAPACITY, capacity, 2)) {
        const unsigned designCapacity = capacity[0] | (capacity[1] << 8);
        if (designCapacity > 0 && designCapacity < 20000 && designCapacity != BATTERY_DESIGN_CAPACITY)
            bq.setDesignCap(BATTERY_DESIGN_CAPACITY);
    }
    // Start with default IR, RF and RFID Configs, replace old
    fluxConfigPins.rfModule = CC1101_SPI_MODULE;
    fluxConfigPins.rfidModule = PN532_I2C_MODULE;
    fluxConfigPins.irRx = 1;
    fluxConfigPins.irTx = 2;
#else
    Wire.begin(SYS_I2C_SDA, SYS_I2C_SCL);
    Wire.beginTransmission(0x40);
    if (Wire.endTransmission() == 0) {
        Serial.println("ES7210 Online, No CC1101 version");
        Wire.end();
    } else {
        Serial.println("Probably CC1101 exists");
        fluxConfigPins.CC1101_bus.cs = GPIO_NUM_17;
        fluxConfigPins.CC1101_bus.io0 = GPIO_NUM_18;
        fluxConfigPins.rfModule = CC1101_SPI_MODULE;

        //* If it does not exist, then the CC1101 shield may exist, so there is no need for Wire to exist.
        Wire.endTransmission();
        Wire.end();
    }
    fluxConfigPins.rfidModule = PN532_SPI_MODULE;

#endif

#ifdef T_EMBED_1101
    pinMode(BK_BTN, INPUT);
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) {
        // The wake button belongs to the wake action, never to the next menu.
        while (digitalRead(BK_BTN) == BTN_ACT) delay(10);
    }
#endif
    pinMode(ENCODER_KEY, INPUT);
    pinMode(ENCODER_INA, INPUT_PULLUP);
    pinMode(ENCODER_INB, INPUT_PULLUP);
    encoder = new RotaryDecoder();
    encoder->begin(ENCODER_INA, ENCODER_INB, 2);
}

/***************************************************************************************
** Function name: getBattery()
** Description:   Delivers the battery value from 1-100
***************************************************************************************/
#if defined(USE_BQ27220_VIA_I2C)
static FluxBatteryFilter batteryFilter;
static std::atomic<bool> batteryCharging{false};

// Unlike the legacy gauge helper, preserve transport errors instead of returning data from a short read.
static bool readPowerRegister(uint8_t address, uint8_t reg, uint8_t *data, uint8_t count) {
    Wire.beginTransmission(address);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(address, count) != count) {
        while (Wire.available()) Wire.read();
        return false;
    }
    for (unsigned i = 0; i < count; ++i) data[i] = Wire.read();
    return true;
}

static void updateBattery() {
    static std::atomic_flag reading = ATOMIC_FLAG_INIT;
    if (reading.test_and_set()) return;
    static bool sampled = false;
    static uint32_t sampledAt = 0;
    const uint32_t now = millis();
    if (sampled && now - sampledAt < 1000) { reading.clear(); return; }
    sampled = true;
    sampledAt = now;
    uint8_t soc[2] = {}, voltage[2] = {}, status[2] = {}, operation[2] = {}, charger = 0;
    const bool valid = readPowerRegister(BQ27220_I2C_ADDRESS, BQ27220_COMMAND_STATE_CHARGE, soc, 2) &&
                       readPowerRegister(BQ27220_I2C_ADDRESS, BQ27220_COMMAND_VOLT, voltage, 2) &&
                       readPowerRegister(BQ27220_I2C_ADDRESS, BQ27220_COMMAND_BATTERY_ST, status, 2) &&
                       readPowerRegister(BQ27220_I2C_ADDRESS, BQ27220_COMMAND_OPERATION_STATUS, operation, 2);
    batteryFilter.sample(valid, soc[0] | (soc[1] << 8), voltage[0] | (voltage[1] << 8),
                         status[0] | (status[1] << 8), operation[0] | (operation[1] << 8));
#ifdef T_EMBED_1101
    batteryCharging = readPowerRegister(BQ25896_SLAVE_ADDRESS, 0x0b, &charger, 1) &&
                      fluxChargerCharging(charger);
#endif
    reading.clear();
}

int getBattery() { updateBattery(); return batteryFilter.level(); }
#endif
/*********************************************************************
**  Function: setBrightness
**  set brightness value
**********************************************************************/
void _setBrightness(uint8_t brightval) {
    if (brightval == 0) {
        analogWrite(TFT_BL, brightval);
    } else if (brightval > 99) {
        analogWrite(TFT_BL, 254);
    } else {
        int bl = MINBRIGHT + round(((255 - MINBRIGHT) * brightval / 100));
        analogWrite(TFT_BL, bl);
    }
}

/*********************************************************************
** Function: InputHandler
** Handles the variables PrevPress, NextPress, SelPress, AnyKeyPress and EscPress
**********************************************************************/
void InputHandler(void) {
    static unsigned long tm = millis();  // debounce for buttons
    static unsigned long tm2 = millis(); // delay between Select and encoder (avoid missclick)
    static unsigned long lastEncoderMoveMs = 0;
    static int posDifference = 0;
    static int lastPos = 0;
    static bool waitForWakeRelease = false;
#ifdef SAKURA_BUILD
    static delta::ButtonInput selectButton, backButton;
    static bool selectLong = false, backLong = false;
    static bool keyboardSelectBlocked = false;
    static uint32_t selectAt = 0, backAt = 0;
#endif
    bool sel = !BTN_ACT;
    bool esc = !BTN_ACT;

    int newPos = encoder->getPosition();
    if (waitForWakeRelease) {
        posDifference = 0;
        lastPos = newPos;
        RotaryNetSteps = 0;
        SelPress = NextPress = PrevPress = EscPress = false;
        bool released = digitalRead(SEL_BTN) != BTN_ACT;
#ifdef T_EMBED_1101
        released = released && digitalRead(BK_BTN) != BTN_ACT;
#endif
        if (released) {
            waitForWakeRelease = false; tm = tm2 = millis();
#ifdef SAKURA_BUILD
            selectButton = {}; backButton = {}; selectLong = backLong = false;
#endif
        }
        return;
    }

    if (DiscardEncoderInput) {
        // A boot/menu boundary has consumed the action. Keep the quadrature
        // baseline aligned with the physical encoder so the same movement is
        // not emitted again when normal menu input resumes.
        posDifference = 0;
        lastPos = newPos;
        lastEncoderMoveMs = millis();
        RotaryNetSteps = 0;
        NextPress = false;
        PrevPress = false;
        SelPress = false;
        AnyKeyPress = false;
        return;
    }

    if (newPos != lastPos) {
#ifdef SAKURA_BUILD
        inputDiagnosticEvent(InputSource::Encoder, newPos > lastPos ? InputKind::Left : InputKind::Right, 0);
#endif
#ifdef FLUX_DELTA_BUILD
        deltaUiPhysicalInput(micros());
#endif
        posDifference += (newPos - lastPos);
        // Independent running total for consumers that want to apply the
        // full pending backlog in one pass instead of one step at a time
        // (see drainRotarySteps() in globals.h). Never cleared by the
        // stale-drop below -- it's drained exactly, not time-limited.
        RotaryNetSteps += (newPos - lastPos);
        lastPos = newPos;
        lastEncoderMoveMs = millis();
    } else if (posDifference != 0 && millis() - lastEncoderMoveMs > 30) {
        // Drop any stale queued steps once the encoder has stopped moving.
        posDifference = 0;
    }

#ifdef SAKURA_BUILD
    const uint32_t sampledAt = millis();
    const int selectEdge = selectButton.sample(digitalRead(SEL_BTN) == BTN_ACT, sampledAt);
    const int backEdge = backButton.sample(digitalRead(BK_BTN) == BTN_ACT, sampledAt);
    auto observe = [&](delta::ButtonInput &button, int edge, InputSource source, uint32_t &pressedAt, bool &longReported) {
        if (edge) {
            inputDiagnosticEvent(source, edge > 0 ? InputKind::Press : InputKind::Release,
                                 uint32_t(sampledAt - button.changedAt) * 1000);
            if (edge > 0) { pressedAt = sampledAt; longReported = false; }
        }
        if (button.down && !longReported && uint32_t(sampledAt - pressedAt) >= 700) {
            longReported = true;
            inputDiagnosticEvent(source, InputKind::Long, uint32_t(sampledAt - pressedAt) * 1000);
        }
    };
    observe(selectButton, selectEdge, InputSource::Select, selectAt, selectLong);
    observe(backButton, backEdge, InputSource::Back, backAt, backLong);
    // A Select held during a power gesture belongs to that gesture until released.
    if (keyboardInputActive() && backButton.down && sampledAt - backAt >= 500)
        keyboardSelectBlocked = true;
    if (!selectButton.raw && !backButton.raw) keyboardSelectBlocked = false;
    if (selectEdge > 0 && !keyboardSelectBlocked) sel = BTN_ACT;
    if (backEdge > 0 && !keyboardInputActive()) esc = BTN_ACT;
#else
    if (millis() - tm > 200 || LongPress) {
        sel = digitalRead(SEL_BTN);
#ifdef T_EMBED_1101
        esc = digitalRead(BK_BTN);
#endif
    }
#endif
    if (posDifference != 0 || sel == BTN_ACT || esc == BTN_ACT) {
        if (isSleeping) {
            AnyKeyPress = true; // Only wake the software sleep menu; never emit Select/Back.
            SelPress = NextPress = PrevPress = EscPress = false;
            waitForWakeRelease = true;
            posDifference = 0;
            RotaryNetSteps = 0;
            return;
        }
        if (!wakeUpScreen()) AnyKeyPress = true;
        else {
            waitForWakeRelease = true;
            posDifference = 0;
            RotaryNetSteps = 0;
            SelPress = NextPress = PrevPress = EscPress = AnyKeyPress = false;
            tm = tm2 = millis();
            return;
        }
    }
    if (posDifference > 0) {
        PrevPress = true;
        posDifference--;
#ifdef HAS_ENCODER_LED
        EncoderLedChange = -1;
#endif
        tm2 = millis();
    }
    if (posDifference < 0) {
        NextPress = true;
        posDifference++;
#ifdef HAS_ENCODER_LED
        EncoderLedChange = 1;
#endif
        tm2 = millis();
    }

    if (sel == BTN_ACT
#ifndef SAKURA_BUILD
        && millis() - tm2 > 200
#endif
        ) {
        posDifference = 0;
        SelPress = true;
        tm = millis();
    }
    if (esc == BTN_ACT) {
#ifdef SAKURA_BUILD
        // A cancel edge wins over an unconsumed selection or rotary burst.
        SelPress = NextPress = PrevPress = false;
        RotaryNetSteps = 0;
        posDifference = 0;
#endif
        AnyKeyPress = true;
        EscPress = true;
        // Serial.println("EscPressed");
        tm = millis();
    }
}

void powerOff() {
#ifdef T_EMBED_1101
    PPM.shutdown();
#endif
}

void powerDownNFC() {
    Adafruit_PN532 nfc = Adafruit_PN532(17, 45);
    bool i2c_check = check_i2c_address(PN532_I2C_ADDRESS);
    nfc.setInterface(SYS_I2C_SDA, SYS_I2C_SCL);
    nfc.begin();
    uint32_t versiondata = nfc.getFirmwareVersion();
    if (i2c_check || versiondata) {
        nfc.powerDown();
    } else {
        Serial.println("Can't powerDown PN532");
    }
}

void powerDownCC1101() {
    if (!initRfModule("rx", fluxConfigPins.rfFreq)) { Serial.println("Can't init CC1101"); }

    ELECHOUSE_cc1101.goSleep();
}

/*********************************************************************
** Function: checkReboot
** location: mykeyboard.cpp
** Btn logic to restart the device (ESP.restart) or deep sleep
**********************************************************************/
void checkReboot() {
#ifdef T_EMBED_1101
    // Early exit if button not pressed
    if (digitalRead(BK_BTN) != BTN_ACT) return;

    // Constants for better readability
    const int SLEEP_TIMEOUT = 3;
    const int RESTART_TIMEOUT = 5;
    const int DISPLAY_DELAY = 500;
    const char *SLEEP_TEXT = "DEEP SLEEP IN 3/3";
    const char *RESTART_TEXT = "RESTART IN 5/5";

    // Calculate banner dimensions once
    const int maxTextWidth = tft.textWidth(SLEEP_TEXT, 1);
    const int bannerX = tftWidth / 2 - maxTextWidth / 2 - 5;
    const int bannerY = 12;
    const int bannerWidth = maxTextWidth + 10;
    const int bannerHeight = tft.fontHeight(1);

    // Helper function to clear banner area
    auto clearBanner = [&]() { tft.fillRect(bannerX, 7, bannerWidth, 18, fluxConfig.bgColor); };

    // Helper function to clear text line only
    auto clearTextLine = [&]() {
        tft.fillRect(bannerX, bannerY, bannerWidth, bannerHeight, fluxConfig.bgColor);
    };

    uint32_t time_count = millis();
    bool isRestartMode = false;
    bool previousMode = false;
    int countDown = 0;
    bool bannerInitialized = false;

    while (digitalRead(BK_BTN) == BTN_ACT) {
        // Check if SEL_BTN is pressed for restart mode
        isRestartMode = (digitalRead(SEL_BTN) == BTN_ACT);

        // Handle mode change: reset timer and clear display
        if (isRestartMode != previousMode && bannerInitialized) {
            clearBanner();
            time_count = millis();
            countDown = 0;
            bannerInitialized = false;
            previousMode = isRestartMode;
            delay(50);
            continue;
        }

        previousMode = isRestartMode;

        // Only show countdown after initial delay
        if (millis() - time_count <= DISPLAY_DELAY) {
            delay(10);
            continue;
        }

        // Initialize banner on first display
        if (!bannerInitialized) {
            clearBanner();
            bannerInitialized = true;
        }

        // Calculate current countdown value
        int newCountDown = (millis() - time_count - DISPLAY_DELAY) / 1000 + 1;

        // Only update display if countdown changed OR mode just initialized
        if (newCountDown != countDown || !bannerInitialized) {
            countDown = newCountDown;

            // Initialize banner on first display
            if (!bannerInitialized) {
                clearBanner();
                bannerInitialized = true;
            }

            tft.setTextSize(1);

            if (isRestartMode) {
                // RESTART MODE: 5 seconds
                if (countDown >= RESTART_TIMEOUT + 1) {
                    // Execute restart
                    tft.fillScreen(fluxConfig.bgColor);
                    tft.setTextColor(fluxConfig.secColor, fluxConfig.bgColor);
                    tft.drawCentreString("RESTARTING...", tftWidth / 2, tftHeight / 2, 2);
                    delay(1000);
                    ESP.restart();
                }

                // Display countdown
                tft.setTextColor(fluxConfig.secColor, fluxConfig.bgColor);
                clearTextLine();
                tft.drawCentreString(
                    "RESTART IN " + String(countDown) + "/" + String(RESTART_TIMEOUT),
                    tftWidth / 2,
                    bannerY,
                    1
                );

            } else {
                // DEEP SLEEP MODE: Normal text, 3 seconds
                if (countDown >= SLEEP_TIMEOUT + 1) {
                    // Execute deep sleep
                    isSleeping = true;
                    isScreenOff = true;
                    setBrightness(0, false);
#ifdef FLUX_DELTA_BUILD
                    deltaUiInvalidate();
#endif
                    panelSleep(true);
                    while (digitalRead(BK_BTN) == BTN_ACT) delay(10);
                    delay(200);
                    WiFi.disconnect(true, false);
                    WiFi.mode(WIFI_OFF);
                    stopBLEStack();
                    // Other BLE entry points do not all pass through stopBLEStack's scan state.
                    NimBLEDevice::deinit(false);
                    ledEffects(false);
                    setLedBrightness(0);
                    powerDownNFC();
                    powerDownCC1101();
                    delay(1000); // Delay for debouncing ;)
                    digitalWrite(PIN_POWER_ON, LOW);
                    esp_sleep_enable_ext0_wakeup(GPIO_NUM_6, LOW);
                    esp_deep_sleep_start();
                }

                // Display countdown
                tft.setTextColor(fluxConfig.priColor, fluxConfig.bgColor);
                clearTextLine();
                tft.drawCentreString(
                    "DEEP SLEEP IN " + String(countDown) + "/" + String(SLEEP_TIMEOUT),
                    tftWidth / 2,
                    bannerY,
                    1
                );
            }
        }

        delay(10);
    }

    // Clear banner after button release
    delay(30);
    if (millis() - time_count > DISPLAY_DELAY) {
        clearBanner();
        drawStatusBar();
    }
#endif
}

/***************************************************************************************
** Function name: isCharging()
** Description:   Determines if the device is charging
***************************************************************************************/
#ifdef USE_BQ27220_VIA_I2C
bool isCharging() {
    updateBattery();
    return batteryCharging.load();
}
#else
bool isCharging() { return false; }
#endif
