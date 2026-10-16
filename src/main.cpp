#include "core/main_menu.h"
#include "core/flux_branding.h"
#include <globals.h>

#include "core/bus_HAL.h"
#include "core/powerSave.h"
#include "core/ram_profile.h"
#include "core/serial_commands/cli.h"
#include "core/utils.h"
#include "current_year.h"
#include "esp32-hal-psram.h"
#include "esp_heap_caps.h"
#include "esp_task_wdt.h"
#include "esp_wifi.h"
#include <functional>
#include <string>
#include <vector>
io_expander ioExpander;
FluxConfig fluxConfig;
FluxConfigPins fluxConfigPins;

SerialCli serialCli;
USBSerial USBserial;
SerialDevice *serialDevice = &USBserial;

StartupApp startupApp;
String startupAppJSInterpreterFile = "";

MainMenu mainMenu;
SPIClass sdcardSPI;
#ifdef USE_HSPI_PORT
#ifndef VSPI
#define VSPI FSPI
#endif
SPIClass AUX_SPI(VSPI);
#else
SPIClass AUX_SPI(HSPI);
#endif

// Navigation Variables
volatile bool NextPress = false;
volatile bool PrevPress = false;
volatile bool UpPress = false;
volatile bool DownPress = false;
volatile bool SelPress = false;
volatile bool EscPress = false;
volatile bool AnyKeyPress = false;
volatile bool NextPagePress = false;
volatile bool PrevPagePress = false;
volatile bool LongPress = false;
volatile bool SerialCmdPress = false;
volatile int forceMenuOption = -1;
volatile uint8_t menuOptionType = 0;
String menuOptionLabel = "";
#ifdef HAS_ENCODER_LED
volatile int EncoderLedChange = 0;
#endif

TouchPoint touchPoint;

keyStroke KeyStroke;

volatile int32_t RotaryNetSteps = 0;

#ifdef HAS_ENCODER
volatile bool DiscardEncoderInput = false;
#endif

#ifdef HAS_ENCODER
// Default no-op: boards that define HAS_ENCODER but don't implement
// pollEncoder() (shouldn't happen, but keeps the linker happy either way).
void __attribute__((weak)) pollEncoder(void) {}

// Dedicated, high-priority, tight-cadence task that does nothing but sample
// the rotary encoder A/B lines -- mirrors the Flipper port's input_srv,
// which runs encoder_poll() on its own thread every 4ms, decoupled from
// GUI/app work so the raw quadrature read is never delayed by rendering
// or by whether the previous input event has been consumed yet. Only
// exists on HAS_ENCODER boards; other boards pay zero cost for this.
static void taskEncoderPoll(void *parameter) {
    while (true) {
        pollEncoder();
        vTaskDelay(pdMS_TO_TICKS(4));
    }
}
#endif

TaskHandle_t xHandle;
void __attribute__((weak)) taskInputHandler(void *parameter) {
    auto timer = millis();
    while (true) {
        checkPowerSaveTime();
#if defined(SAKURA_BUILD) && defined(T_EMBED_1101)
        // Debounced edges must stay pending until a consumer takes them.
        InputHandler();
#else
        // Sometimes this task run 2 or more times before looptask,
        // and navigation gets stuck, the idea here is run the input detection
        // if AnyKeyPress is false, or rerun if it was not renewed within 75ms (arbitrary)
        // because AnyKeyPress will be true if didn´t passed through a check(bool var)
        if (!AnyKeyPress || millis() - timer > 75) {
            NextPress = false;
            PrevPress = false;
            UpPress = false;
            DownPress = false;
            SelPress = false;
            EscPress = false;
            AnyKeyPress = false;
            SerialCmdPress = false;
            NextPagePress = false;
            PrevPagePress = false;
            touchPoint.pressed = false;
            touchPoint.Clear();
            checkAndRecoverSysI2CBus();
#ifndef USE_TFT_eSPI_TOUCH
            InputHandler();
#endif
            timer = millis();
        }
#endif
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
// Public Globals Variables
unsigned long previousMillis = millis();
int prog_handler; // 0 - Flash, 1 - LittleFS, 3 - Download
String cachedPassword = "";
int8_t interpreter_state = -1;
bool sdcardMounted = false;
bool gpsConnected = false;

// wifi globals
// TODO put in a namespace
bool wifiConnected = false;
bool isWebUIActive = false;
String wifiIP;

bool BLEConnected = false;
bool returnToMenu;
bool isSleeping = false;
bool isScreenOff = false;
bool dimmer = false;
char timeStr[16];
time_t localTime;
struct tm *timeInfo;
#if defined(HAS_RTC)
#if defined(HAS_RTC_PCF85063A)
pcf85063_RTC _rtc;
#else
cplus_RTC _rtc;
#endif
RTC_TimeTypeDef _time;
RTC_DateTypeDef _date;
bool clock_set = true;
#else
ESP32Time rtc;
bool clock_set = false;
#endif

std::vector<Option> options;
// Protected global variables
#if defined(HAS_SCREEN)
tft_logger tft = tft_logger(); // Invoke custom library
tft_sprite sprite = tft_sprite(&tft);
tft_sprite draw = tft_sprite(&tft);
volatile int tftWidth = TFT_HEIGHT;
#ifdef HAS_TOUCH
volatile int tftHeight =
    TFT_WIDTH - 20; // 20px to draw the TouchFooter(), were the btns are being read in touch devices.
#else
volatile int tftHeight = TFT_WIDTH;
#endif
#else
tft_logger tft;
SerialDisplayClass &sprite = tft;
SerialDisplayClass &draw = tft;
volatile int tftWidth = VECTOR_DISPLAY_DEFAULT_HEIGHT;
volatile int tftHeight = VECTOR_DISPLAY_DEFAULT_WIDTH;
#endif

#include "core/bus_HAL.h"
#include "core/display.h"
#include "core/led_control.h"
#include "core/mykeyboard.h"
#include "core/sd_functions.h"
#include "core/serialcmds.h"
#include "core/settings.h"
#include "core/wifi/webInterface.h"
#include "core/wifi/wifi_common.h"
#include "modules/bjs_interpreter/interpreter.h" // for JavaScript interpreter
#include "modules/others/audio.h"                // for playAudioFile
#include "modules/rf/rf_utils.h"                 // for initCC1101once
#include <Wire.h>

/*********************************************************************
 **  Function: begin_storage
 **  Config LittleFS and SD storage
 *********************************************************************/
void begin_storage() {
    const bool littleFsReady = setupLittleFS();
    if (!littleFsReady) Serial.println("LittleFS mount failed; existing data preserved");
    RAM_LOG("after LittleFS");
    bool checkFS = setupSdCard();
    if (!littleFsReady && !checkFS) {
        Serial.println("Storage unavailable; using in-memory defaults without saving");
        return;
    }
    fluxConfig.fromFile(checkFS);
    fluxConfigPins.fromFile(checkFS);
}

/*********************************************************************
 **  Function: _setup_gpio()
 **  Sets up a weak (empty) function to be replaced by /ports/* /interface.h
 *********************************************************************/
void _setup_gpio() __attribute__((weak));
void _setup_gpio() {}

/*********************************************************************
 **  Function: _post_setup_gpio()
 **  Sets up a weak (empty) function to be replaced by /ports/* /interface.h
 *********************************************************************/
void _post_setup_gpio() __attribute__((weak));
void _post_setup_gpio() {}

/*********************************************************************
 **  Function: _pre_storage_gpio()
 **  Sets up a weak (empty) function for board fixes that must run
 **  after the first TFT access and before storage is mounted.
 *********************************************************************/
void _pre_storage_gpio() __attribute__((weak));
void _pre_storage_gpio() {}

/*********************************************************************
 **  Function: setup_gpio
 **  Setup GPIO pins
 *********************************************************************/
void setup_gpio() {

    // init setup from /ports/*/interface.h
    _setup_gpio();

    // Smoochiee v2 uses a AW9325 tro control GPS, MIC, Vibro and CC1101 RX/TX powerlines
    ioExpander.init(IO_EXPANDER_ADDRESS, &Wire);

    initCC1101once(acquireSPIBus(
        fluxConfigPins.CC1101_bus.sck, fluxConfigPins.CC1101_bus.miso, fluxConfigPins.CC1101_bus.mosi
    ));
    // acquireSPIBus() returns nullptr when these pins have no hardware controller left (e.g.
    // ARDUINO_M5STICK_C_PLUS and others that don't share SPI with the display/SD/aux bus);
    // initCC1101once(NULL) lets the driver fall back to managing the default SPI object itself.
}

/*********************************************************************
 **  Function: begin_tft
 **  Config tft
 *********************************************************************/
void begin_tft() {
    tft.setRotation(fluxConfigPins.rotation); // sometimes it misses the first command
    tft.invertDisplay(fluxConfig.colorInverted);
    tft.setRotation(fluxConfigPins.rotation);
    tftWidth = tft.width();
#ifdef HAS_TOUCH
    tftHeight = tft.height() - 20;
#else
    tftHeight = tft.height();
#endif
#ifdef SAKURA_BUILD
    resetTftDisplay(0, 0, tft.color565(235, 161, 190), FM, TFT_BLACK, TFT_BLACK);
    tft.drawCentreString("SAKURA", tftWidth / 2, tftHeight / 2 - 14, 1);
    tft.setTextSize(FP);
    tft.drawCentreString("SAKURA NIGHT", tftWidth / 2, tftHeight / 2 + 12, 1);
#else
    resetTftDisplay();
#endif
    setBrightness(fluxConfig.bright, false);
}

/*********************************************************************
 **  Function: boot_screen
 **  Draw boot screen
 *********************************************************************/
void boot_screen() {
    tft.setTextColor(fluxConfig.priColor, fluxConfig.bgColor);
    tft.setTextSize(FM);
    tft.drawPixel(0, 0, fluxConfig.bgColor);
    tft.drawCentreString("Flux", tftWidth / 2, 10, 1);
    tft.setTextSize(FP);
    tft.drawCentreString(FLUX_VERSION, tftWidth / 2, 25, 1);
    tft.setTextSize(FM);
    tft.drawCentreString(
        "PREDATORY FIRMWARE", tftWidth / 2, tftHeight + 2, 1
    ); // will draw outside the screen on non touch devices
}

/*********************************************************************
 **  Function: boot_screen_anim
 **  Draw boot screen
 *********************************************************************/
void boot_screen_anim() {
#ifdef FLUX_BUILD
    // Every boot plays each embedded private image once, then waits for a new
    // physical Select press. No image index is persisted between boots.
    // Input is sampled between frames so one encoder action can skip the
    // complete slideshow rather than merely advancing one image.
    auto bootInputRequested = []() -> bool {
#if defined(HAS_ENCODER)
        return RotaryNetSteps != 0 || NextPress || PrevPress || SelPress || digitalRead(SEL_BTN) == BTN_ACT;
#else
        return AnyKeyPress;
#endif
    };
    auto waitForBootInput = [&](uint32_t durationMs) -> bool {
        const uint32_t startedAt = millis();
        while (millis() - startedAt < durationMs) {
            if (bootInputRequested()) return true;
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        return bootInputRequested();
    };
    auto discardBootInput = [&]() {
#if defined(HAS_ENCODER)
        // Give the high-priority encoder sampler a few ticks to observe the
        // final position. Its board-side handler clears its private pending
        // delta while this guard is active and resynchronises its baseline.
        DiscardEncoderInput = true;
        const uint32_t startedAt = millis();
        do {
            clearInputState();
            vTaskDelay(pdMS_TO_TICKS(5));
        } while (millis() - startedAt < 50 || digitalRead(SEL_BTN) == BTN_ACT);
        clearInputState();
        DiscardEncoderInput = false;
#else
        clearInputState();
#endif
    };

    clearInputState();
    const uint8_t count = fluxPrivateImageCount();
    bool drewPrivateImage = false;
    bool skipSlideshow = false;
    for (uint8_t index = 0; index < count; ++index) {
        if (index > 0) {
            tft.fillScreen(TFT_BLACK);
            if (waitForBootInput(120)) {
                skipSlideshow = true;
                break;
            }
        }
        if (!drawFluxPrivateBranding(index)) break;
        drewPrivateImage = true;
        Serial.printf("[FLUX boot] private image %u/%u\n", index + 1, count);
        if (bootInputRequested()) {
            skipSlideshow = true;
            break;
        }
        if (index + 1 < count && waitForBootInput(1600)) {
            skipSlideshow = true;
            break;
        }
    }
    if (!drewPrivateImage) {
        drawFluxEmbeddedBranding("flux.splash.png");
        if (bootInputRequested()) skipSlideshow = true;
    }

    if (skipSlideshow) {
        Serial.println("[FLUX boot] private slideshow skipped");
        discardBootInput();
        tft.fillScreen(fluxConfig.bgColor);
        return;
    }

    // A power/wake press must be released before it can count as the explicit
    // post-slideshow confirmation.
    clearInputState();
#if defined(HAS_ENCODER)
    while (digitalRead(SEL_BTN) == BTN_ACT) vTaskDelay(pdMS_TO_TICKS(10));
#endif
    clearInputState();
    while (!check(SelPress)) vTaskDelay(pdMS_TO_TICKS(10));
    clearInputState();
    tft.fillScreen(fluxConfig.bgColor);
    return;
#endif
    boot_screen();
#ifdef FLUX_BUILD
    // One static transition draw inside the existing boot interval; no frame loop or added delay.
    if (drawFluxBrandingAsset("flux.secondary.png")) Serial.println("FLUX secondary branding asset");
#endif
    int i = millis();
    // checks for boot.jpg in SD and LittleFS for customization
    int boot_img = 0;
    bool drawn = false;
#ifdef FLUX_BUILD
    // FLUX branding pack takes precedence; legacy Flux /boot.* remains a fallback.
    if (sdcardMounted) {
        if (SD.exists("/flux/assets/branding/flux.splash.jpg")) boot_img = 6;
        else if (SD.exists("/flux/assets/branding/flux.splash.gif")) boot_img = 7;
        else if (SD.exists("/flux/assets/branding/flux.splash.png")) boot_img = 8;
    }
    if (boot_img == 0) {
        if (LittleFS.exists("/flux/assets/branding/flux.splash.jpg")) boot_img = 9;
        else if (LittleFS.exists("/flux/assets/branding/flux.splash.gif")) boot_img = 10;
        else if (LittleFS.exists("/flux/assets/branding/flux.splash.png")) boot_img = 11;
    }
#endif
    if (sdcardMounted) {
        if (boot_img == 0 && SD.exists("/boot.jpg")) boot_img = 1;
        else if (boot_img == 0 && SD.exists("/boot.gif")) boot_img = 3;
    }
    if (boot_img == 0 && LittleFS.exists("/boot.jpg")) boot_img = 2;
    else if (boot_img == 0 && LittleFS.exists("/boot.gif")) boot_img = 4;
    if (fluxConfig.theme.boot_img) boot_img = 5; // override others

    tft.drawPixel(0, 0, 0);       // Forces back communication with TFT, to avoid ghosting
                                  // Start image loop
    while (millis() < i + 7000) { // boot image lasts for 5 secs
        if ((millis() - i > 2000) && !drawn) {
            tft.fillRect(0, 45, tftWidth, tftHeight - 45, fluxConfig.bgColor);
            if (boot_img > 0 && !drawn) {
                tft.fillScreen(fluxConfig.bgColor);
                if (boot_img == 5) {
                    drawImg(
                        *fluxConfig.themeFS(),
                        fluxConfig.getThemeItemImg(fluxConfig.theme.paths.boot_img),
                        0,
                        0,
                        true,
                        3600
                    );
                    Serial.println("Image from SD theme");
                } else if (boot_img == 1) {
                    drawImg(SD, "/boot.jpg", 0, 0, true);
                    Serial.println("Image from SD");
                } else if (boot_img == 2) {
                    drawImg(LittleFS, "/boot.jpg", 0, 0, true);
                    Serial.println("Image from LittleFS");
                } else if (boot_img == 3) {
                    drawImg(SD, "/boot.gif", 0, 0, true, 3600);
                    Serial.println("Image from SD");
                } else if (boot_img == 4) {
                    drawImg(LittleFS, "/boot.gif", 0, 0, true, 3600);
                    Serial.println("Image from LittleFS");
#ifdef FLUX_BUILD
                } else if (boot_img >= 6 && boot_img <= 8) {
                    const char *path = boot_img == 6 ? "/flux/assets/branding/flux.splash.jpg" :
                                       boot_img == 7 ? "/flux/assets/branding/flux.splash.gif" :
                                                       "/flux/assets/branding/flux.splash.png";
                    drawImg(SD, path, 0, 0, true, boot_img == 7 ? 3600 : 0);
                    Serial.println("FLUX splash from SD");
                } else if (boot_img >= 9 && boot_img <= 11) {
                    const char *path = boot_img == 9 ? "/flux/assets/branding/flux.splash.jpg" :
                                       boot_img == 10 ? "/flux/assets/branding/flux.splash.gif" :
                                                        "/flux/assets/branding/flux.splash.png";
                    drawImg(LittleFS, path, 0, 0, true, boot_img == 10 ? 3600 : 0);
                    Serial.println("FLUX splash from LittleFS");
#endif
                }
                tft.drawPixel(0, 0, 0); // Forces back communication with TFT, to avoid ghosting
            }
            drawn = true;
        }
#if !defined(LITE_VERSION)
        if (!boot_img && (millis() - i > 2200) && (millis() - i) < 2700)
            tft.drawRect(2 * tftWidth / 3, tftHeight / 2, 2, 2, fluxConfig.priColor);
        if (!boot_img && (millis() - i > 2700) && (millis() - i) < 2900)
            tft.fillRect(0, 45, tftWidth, tftHeight - 45, fluxConfig.bgColor);
        if (!boot_img && (millis() - i > 2900) && (millis() - i) < 3400)
            tft.drawXBitmap(
                2 * tftWidth / 3 - 30,
                5 + tftHeight / 2,
                flux_small_bits,
                flux_small_width,
                flux_small_height,
                fluxConfig.bgColor,
                fluxConfig.priColor
            );
        if (!boot_img && (millis() - i > 3400) && (millis() - i) < 3600) tft.fillScreen(fluxConfig.bgColor);
        if (!boot_img && (millis() - i > 3600))
            tft.drawXBitmap(
                (tftWidth - 238) / 2,
                (tftHeight - 133) / 2,
                bits,
                bits_width,
                bits_height,
                fluxConfig.bgColor,
                fluxConfig.priColor
            );
#endif
        if (check(AnyKeyPress)) // If any key or M5 key is pressed, it'll jump the boot screen
        {
            tft.fillScreen(fluxConfig.bgColor);
            delay(10);
            return;
        }
    }

    // Clear splashscreen
    tft.fillScreen(fluxConfig.bgColor);
}

/*********************************************************************
 **  Function: init_clock
 **  Clock initialisation for propper display in menu
 *********************************************************************/
void init_clock() {
#if defined(HAS_RTC)
    _rtc.begin();
#if defined(HAS_RTC_BM8563)
    _rtc.GetBm8563Time();
#endif
#if defined(HAS_RTC_PCF85063A)
    _rtc.GetPcf85063Time();
#endif
    _rtc.GetTime(&_time);
    _rtc.GetDate(&_date);

    struct tm timeinfo = {};
    timeinfo.tm_sec = _time.Seconds;
    timeinfo.tm_min = _time.Minutes;
    timeinfo.tm_hour = _time.Hours;
    timeinfo.tm_mday = _date.Date;
    timeinfo.tm_mon = _date.Month > 0 ? _date.Month - 1 : 0;
    timeinfo.tm_year = _date.Year >= 1900 ? _date.Year - 1900 : 0;
    time_t epoch = mktime(&timeinfo);
    struct timeval tv = {.tv_sec = epoch};
    settimeofday(&tv, nullptr);
#else
    struct tm timeinfo = {};
    timeinfo.tm_year = CURRENT_YEAR - 1900;
    timeinfo.tm_mon = 0x05;
    timeinfo.tm_mday = 0x14;
    time_t epoch = mktime(&timeinfo);
    rtc.setTime(epoch);
    clock_set = true;
    struct timeval tv = {.tv_sec = epoch};
    settimeofday(&tv, nullptr);
    restorePersistedClock(); // override the default with the last-saved time (NVS) + start periodic save
#endif
}

/*********************************************************************
 **  Function: init_led
 **  Led initialisation
 *********************************************************************/
void init_led() {
#ifdef HAS_RGB_LED
    beginLed();
#endif
}

/*********************************************************************
 **  Function: startup_sound
 **  Play sound or tone depending on device hardware
 *********************************************************************/
void startup_sound() {
    if (fluxConfig.soundEnabled == 0) return; // if sound is disabled, do not play sound
#if !defined(LITE_VERSION)
#if defined(BUZZ_PIN)
    // Bip M5 just because it can. Does not bip if splashscreen is bypassed
    _tone(5000, 50);
    delay(200);
    _tone(5000, 50);
    /*  2fix: menu infinite loop */
#elif defined(HAS_NS4168_SPKR)
    // play a boot sound
    if (fluxConfig.theme.boot_sound) {
        playAudioFile(fluxConfig.themeFS(), fluxConfig.getThemeItemImg(fluxConfig.theme.paths.boot_sound));
    } else if (SD.exists("/boot.wav")) {
        playAudioFile(&SD, "/boot.wav");
    } else if (LittleFS.exists("/boot.wav")) {
        playAudioFile(&LittleFS, "/boot.wav");
    }
#endif
#endif
}

void begin_startup_app() {
    if (fluxConfig.startupApp == "") return;
#ifdef FLUX_DELTA_BUILD
    bool confirmed = false;
    std::vector<Option> startupOptions = {
        {"Main Menu", []() {}},
        {String("Run: ") + fluxConfig.startupApp, [&confirmed]() { confirmed = true; }}
    };
    loopOptions(startupOptions, MENU_TYPE_SUBMENU, "Saved Startup App", 0);
    if (!confirmed) return;
    startupApp.startApp(fluxConfig.startupApp);
#else
    if (!startupApp.startApp(fluxConfig.startupApp)) fluxConfig.setStartupApp("");
#endif
}

/*********************************************************************
 **  Function: setup
 **  Where the devices are started and variables set
 *********************************************************************/
void setup() {
    Serial.setRxBufferSize(
        SAFE_STACK_BUFFER_SIZE / 4
    ); // Must be invoked before Serial.begin(). Default is 256 chars
    Serial.begin(115200);

    log_d("Total heap: %d", ESP.getHeapSize());
    log_d("Free heap: %d", ESP.getFreeHeap());
    bool psramStarted = psramInit();
    // Printed unconditionally (boards force CORE_DEBUG_LEVEL=1, so log_d is invisible).
    // If PSRAM fails to init, a PSRAM board effectively becomes a no-PSRAM board and
    // Wi-Fi + BLE cannot coexist. This one boot line makes that failure mode observable.
    Serial.printf(
        "[PSRAM] init=%d found=%d total=%u free=%u | internal free=%u largest=%u\n",
        psramStarted,
        psramFound(),
        (unsigned)ESP.getPsramSize(),
        (unsigned)ESP.getFreePsram(),
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)
    );
    Serial.flush();

    RAM_LOG("setup-start");

    // declare variables
    prog_handler = 0;
    sdcardMounted = false;
    wifiConnected = false;
    BLEConnected = false;
    fluxConfig.bright = 100; // theres is no value yet
    fluxConfigPins.rotation = ROTATION;
    setup_gpio();
#if defined(HAS_SCREEN)
    tft.init();
    tft.setRotation(fluxConfigPins.rotation);
    tft.fillScreen(TFT_BLACK);
    // fluxConfig is not read yet.. just to show something on screen due to long boot time
    tft.setTextColor(TFT_PURPLE, TFT_BLACK);
    tft.drawCentreString("Booting", tft.width() / 2, tft.height() / 2, 1);
    RAM_LOG("first-display-elem"); // first element drawn on screen
#else
    tft.begin();
#endif
    _pre_storage_gpio();
    begin_storage();
    RAM_LOG("after-storage"); // fluxConfig/fluxConfigPins loaded from FS
    begin_tft();
    init_clock();
    init_led();
    RAM_LOG("after-tft-clock-led");

    options.reserve(20); // preallocate some options space to avoid fragmentation

    RAM_LOG("before-wifi-init"); // largest contiguous internal block here gates Wi-Fi/BLE

    // Set WiFi country to avoid warnings and ensure max power
    const wifi_country_t country = {
        .cc = "US",
        .schan = 1,
        .nchan = 14,
#ifdef CONFIG_ESP_PHY_MAX_TX_POWER
        .max_tx_power = CONFIG_ESP_PHY_MAX_TX_POWER, // 20
#endif
        .policy = WIFI_COUNTRY_POLICY_MANUAL
    };

    esp_wifi_set_max_tx_power(80); // 80 translates to 20dBm
    esp_wifi_set_country(&country);

    // Some GPIO Settings (such as CYD's brightness control must be set after tft and sdcard)
    _post_setup_gpio();
    // Some board interfaces initialize or reset the backlight in post-setup,
    // so re-apply the stored brightness after that stage completes.
    setBrightness(fluxConfig.bright, false);
    // end of post gpio begin

    // #ifndef USE_TFT_eSPI_TOUCH
    // This task keeps running all the time, will never stop
    xTaskCreate(
        taskInputHandler,              // Task function
        "InputHandler",                // Task Name
        INPUT_HANDLER_TASK_STACK_SIZE, // Stack size
        NULL,                          // Task parameters
        2,                             // Task priority (0 to 3), loopTask has priority 2.
        &xHandle                       // Task handle (not used)
    );
#ifdef HAS_ENCODER
    // Dedicated encoder sampling task, higher priority than loopTask so a
    // busy render/redraw pass can never delay reading the A/B lines.
    // Only created on boards with a rotary encoder.
    xTaskCreate(
        taskEncoderPoll, // Task function
        "EncoderPoll",   // Task Name
        2048,            // Stack size
        NULL,            // Task parameters
        3,               // Task priority (0 to 3), higher than loopTask's 2
        NULL             // Task handle (not used)
    );
#endif
    // #endif
#if defined(HAS_SCREEN)
    fluxConfig.openThemeFile(fluxConfig.themeFS(), fluxConfig.themePath, false);
#ifndef FLUX_DELTA_BUILD
    if (!fluxConfig.instantBoot) {
        boot_screen_anim();
        startup_sound();
    }
#endif
    if (fluxConfig.wifiAtStartup) {
        log_i("Loading Wifi at Startup");
        xTaskCreate(
            wifiConnectTask,   // Task function
            "wifiConnectTask", // Task Name
            4096,              // Stack size
            NULL,              // Task parameters
            2,                 // Task priority (0 to 3), loopTask has priority 2.
            NULL               // Task handle (not used)
        );
    }
#endif
    //  start a task to handle serial commands while the webui is running
    startSerialCommandsHandlerTask(true);

    wakeUpScreen();
    begin_startup_app();

    RAM_LOG("setup-end");
}

/**********************************************************************
 **  Function: loop
 **  Main loop
 **********************************************************************/
#if defined(HAS_SCREEN)
void loop() {
#if !defined(LITE_VERSION) && !defined(DISABLE_INTERPRETER)
    if (interpreter_state > 0) {
        vTaskDelay(pdMS_TO_TICKS(10));
        interpreter_state = 2;
        Serial.println("Entering interpreter...");
        while (interpreter_state > 0) { vTaskDelay(pdMS_TO_TICKS(500)); }
        if (interpreter_state == 0) {
            Serial.println("Interpreter put to background.");
        } else {
            Serial.println("Exiting interpreter...");
        }
        if (interpreter_state == -1) { interpreterTaskHandler = NULL; }
        previousMillis = millis(); // ensure that will not dim screen when get back to menu
    }
#endif
    tft.fillScreen(fluxConfig.bgColor);

#if defined(ENABLE_RAM_LOGGING)
    static bool ramLoggedFirstMenu = false;
    if (!ramLoggedFirstMenu) {
        RAM_LOG("first-mainMenu");
        ramLoggedFirstMenu = true;
    }
#endif

    mainMenu.begin();
    delay(1);
}
#else

void loop() {
    tft.setLogging();
    Serial.println(
        "\n"
        "██████  ██████  ██    ██  ██████ ███████ \n"
        "██   ██ ██   ██ ██    ██ ██      ██      \n"
        "██████  ██████  ██    ██ ██      █████   \n"
        "██   ██ ██   ██ ██    ██ ██      ██      \n"
        "██████  ██   ██  ██████   ██████ ███████ \n"
        "                                         \n"
        "         PREDATORY FIRMWARE\n\n"
        "Tips: Connect to the WebUI for better experience\n"
        "      Add your network by sending: wifi add ssid password\n\n"
        "At your command:"
    );

    // Enable navigation through webUI
    tft.fillScreen(fluxConfig.bgColor);
    mainMenu.begin();
    vTaskDelay(10 / portTICK_PERIOD_MS);
}
#endif
