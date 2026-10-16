#include "ble_common.h"
#include <atomic>
#ifdef USING_NIMBLE_ARDUINO_HEADERS
#include "nimble/porting/nimble/include/nimble/nimble_port.h"
#else
#include "nimble/nimble_port.h"
#endif
#include "core/mykeyboard.h"
#ifdef SAKURA_BUILD
#include "core/input_diagnostics.h"
#endif
#include "core/radio_mem.h"
#include "core/ram_profile.h"
#include "core/utils.h"
#include "core/wifi/wifi_common.h"
#ifdef FLUX_DELTA_BUILD
#include "core/delta_ui.h"
#include <delta/instruments.h>
#endif
#include "esp_mac.h"
#include "modules/badusb_ble/ducky_typer.h"
#if !defined(LITE_VERSION)
#include "BLE_Suite.h"
#endif

#define SERVICE_UUID "1bc68b2a-f3e3-11e9-81b4-2a2ae2dbcce4"
#define CHARACTERISTIC_RX_UUID "1bc68da0-f3e3-11e9-81b4-2a2ae2dbcce4"
#define CHARACTERISTIC_TX_UUID "1bc68efe-f3e3-11e9-81b4-2a2ae2dbcce4"

BLEScan *pBLEScan = nullptr;
int scanTime = SCANTIME;

bool bleNotifyRetry(NimBLECharacteristic *chr, const uint8_t *value, size_t length, uint8_t retries) {
    if (chr == nullptr) return false;
    if (chr->notify(value, length)) return true;
    for (uint8_t i = 0; i < retries; i++) {
        vTaskDelay(1);
        if (chr->notify(value, length)) return true;
    }
    return false;
}

bool bleNotifyRetry(NimBLECharacteristic *chr, uint8_t retries) {
    if (chr == nullptr) return false;
    if (chr->notify()) return true;
    for (uint8_t i = 0; i < retries; i++) {
        vTaskDelay(1);
        if (chr->notify()) return true;
    }
    return false;
}

#define ENDIAN_CHANGE_U16(x) ((((x) & 0xFF00) >> 8) + (((x) & 0xFF) << 8))

BLEServer *pServer = NULL;
BLEService *pService = NULL;
BLECharacteristic *pTxCharacteristic;
BLECharacteristic *pRxCharacteristic;
bool bleDataTransferEnabled = false;

bool deviceConnected = false;
bool oldDeviceConnected = false;

class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer *pServer) { deviceConnected = true; };

    void onDisconnect(BLEServer *pServer) { deviceConnected = false; }
};

class MyCallbacks : public BLECharacteristicCallbacks {
    NimBLEAttValue data;
    void onWrite(NimBLECharacteristic *pCharacteristic) { data = pCharacteristic->getValue(); }
};

uint8_t sta_mac[6];
char strID[18];
char strAddl[200];

void ble_info(const String &name, const String &address, const String &signal) {
#ifdef FLUX_DELTA_BUILD
    if(uint16_t*p=deltaUiCanvas()) {
        delta::ui::RadioRow row(name.c_str(),address.c_str(),int16_t(signal.toInt()));
        delta::ui::radioResults(p,deltaUiAppearance(),"BLE device",&row,1,0,2,&deltaUiStatus());
        deltaUiPresent();
    } else {
#endif
    drawMainBorder();
    tft.setTextColor(fluxConfig.priColor);
    tft.drawCentreString("-=Information=-", tftWidth / 2, 28, SMOOTH_FONT);
    tft.drawString("Name: " + name, 10, 48);
    tft.drawString("Adresse: " + address, 10, 66);
    tft.drawString("Signal: " + String(signal) + " dBm", 10, 84);
    tft.drawCentreString("   Press " + String(BTN_ALIAS) + " to act", tftWidth / 2, tftHeight - 20, 1);
#ifdef FLUX_DELTA_BUILD
    }
#endif

    while (true) {
        if (check(EscPress)) return;
        if (check(SelPress)) return;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

class AdvertisedDeviceCallbacks : public NimBLEScanCallbacks {};

static AdvertisedDeviceCallbacks g_scanCallbacks;

static bool is_ble_inited = false;

bool bleScanCancellable(NimBLEScan *scan, uint32_t durationMs, bool continueScan, bool stopOnSelect) {
    if (!scan) return false;
    if (check(EscPress)) return false;
    if (scan->isScanning()) {
        displayError("BLE scanner busy");
        return false;
    }
#ifdef SAKURA_BUILD
    inputDiagnosticScreen("ble.scan");
    inputDiagnosticAction("scan", "waiting");
#endif
    // Callers use static/inert callbacks; preserve their duplicate-filter settings.
    if (!scan->start(durationMs, continueScan, false)) {
        displayError("BLE scan start failed");
        return false;
    }
    const uint32_t started = millis();
    bool cancelled = false;
    bool selected = false;
    while (scan->isScanning()) {
        if (check(EscPress)) {
            cancelled = true;
            break;
        }
        if (stopOnSelect && check(SelPress)) {
            selected = true;
            break;
        }
        if (millis() - started >= durationMs + 1000) {
            displayError("BLE scan timed out");
            cancelled = true;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    // Catch Back at completion before a results screen or the next scan starts.
    if (check(EscPress)) cancelled = true;
    if (cancelled || selected) {
#ifdef SAKURA_BUILD
        inputDiagnosticAction("scan", cancelled ? "cancelling" : "stopping");
#endif
        displayTextLine(cancelled ? "Cancelling..." : "Stopping capture...");
        const bool stopped = scan->stop();
        const uint32_t stopping = millis();
        while (scan->isScanning() && millis() - stopping < 250) vTaskDelay(pdMS_TO_TICKS(10));
        if (!stopped || scan->isScanning()) {
#ifdef SAKURA_BUILD
            inputDiagnosticAction("scan", "stop failed");
#endif
            displayError("BLE scan stop failed");
            // Retain the operation's error screen until the controller really stops.
            // A new Back retries cancellation; no task is deleted and no foreign radio is reset.
            while (scan->isScanning()) {
                if (check(EscPress)) scan->stop();
                vTaskDelay(pdMS_TO_TICKS(10));
            }
            scan->clearResults();
#ifdef SAKURA_BUILD
            inputDiagnosticAction("scan", "stopped after error");
            inputDiagnosticScreen("menu");
#endif
            clearInputState();
            return false;
        }
        if (cancelled) scan->clearResults();
    }
#ifdef SAKURA_BUILD
    inputDiagnosticAction("scan", cancelled ? "cancelled" : "completed");
    inputDiagnosticScreen("menu");
#endif
    // Events observed on a waiting/error screen cannot activate its parent/results.
    clearInputState();
    return !cancelled;
}

bool bleClientOperation(NimBLEClient *client, const std::function<bool()> &operation,
                        const char *phase, bool &cancelled, bool disconnectAfter) {
    if (check(EscPress)) cancelled = true;
    if (!client || (cancelled && !disconnectAfter)) return false;
    const auto config = client->getConfig();
    if (config.deleteOnDisconnect || config.deleteOnConnectFail) {
        displayError("BLE client lifetime unavailable");
        return false;
    }
    struct Job {
        std::function<bool()> operation;
        std::atomic<bool> done{false};
        bool result = false;
        explicit Job(const std::function<bool()> &fn) : operation(fn) {}
    } job(operation);
    displayTextLine(phase);
    if (disconnectAfter) {
        job.result = true;
        job.done.store(true, std::memory_order_release);
    } else if (xTaskCreate([](void *context) {
            Job *job = static_cast<Job *>(context);
            job->result = job->operation();
            job->done.store(true, std::memory_order_release);
            vTaskDelete(nullptr); // Only the finished worker deletes itself.
        }, "ble-client", 6144, &job, 1, nullptr) != pdPASS) {
        displayError("BLE worker unavailable");
        return false;
    }
    bool disconnectRequested = false;
    while (true) {
        while (true) {
            if (check(EscPress)) {
                cancelled = true;
                disconnectRequested = false;
                displayTextLine("Cancelling...");
            }
            const bool done = job.done.load(std::memory_order_acquire);
            if ((cancelled || disconnectAfter) && client->getConnHandle() != BLE_HS_CONN_HANDLE_NONE && !disconnectRequested) {
                disconnectRequested = client->disconnect();
                if (!disconnectRequested) {
                    displayError("BLE disconnect failed; Back retries");
                    disconnectRequested = true;
                }
            }
            // Never abandon a task/client/context while NimBLE is still using it.
            // ponytail: pending connect uses its finite timeout; cancelConnect is global.
            if (done && (!(cancelled || disconnectAfter) || client->getConnHandle() == BLE_HS_CONN_HANDLE_NONE)) break;
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        // GAP publishes NONE before its final client access. Join the host queue
        // as well as the worker before a caller may delete this client.
        const bool cancelledBeforeFence = cancelled;
        ble_npl_event barrier{};
        std::atomic<bool> hostReleased{false};
        ble_npl_event_init(&barrier, [](ble_npl_event *event) {
            auto *released = static_cast<std::atomic<bool> *>(ble_npl_event_get_arg(event));
            // Last context access: the installed dispatcher does not dereference
            // the event after this callback; nimble_port_run only compares it.
            released->store(true, std::memory_order_release);
        }, &hostReleased);
        ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &barrier);
        const uint32_t fenceStarted = millis();
        bool waitReported = false;
        while (!hostReleased.load(std::memory_order_acquire)) {
            if (check(EscPress)) {
                cancelled = true;
                disconnectRequested = false;
                displayTextLine("Cancelling...");
            }
            if (!waitReported && millis() - fenceStarted >= 1000) {
                displayError("Waiting for BLE cleanup");
                waitReported = true;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        ble_npl_event_deinit(&barrier);
        if (check(EscPress)) {
            cancelled = true;
            disconnectRequested = false;
            displayTextLine("Cancelling...");
        }
        // Back during this fence requires an owned disconnect and a new fence
        // posted after NONE, even when disconnection completed concurrently.
        if ((!cancelledBeforeFence && cancelled) ||
            (cancelled && client->getConnHandle() != BLE_HS_CONN_HANDLE_NONE)) continue;
        break;
    }
    clearInputState();
    return (disconnectAfter || !cancelled) && job.result;
}

bool bleClientOperation(NimBLEClient *client, const std::function<bool()> &operation,
                        const char *phase, bool &cancelled) {
    return bleClientOperation(client, operation, phase, cancelled, false);
}

bool bleClientDisconnect(NimBLEClient *client, bool &cancelled) {
    return bleClientOperation(client, []() { return true; }, "Disconnecting...", cancelled, true);
}

void stopBLEStack() {
    if (pBLEScan) {
        pBLEScan->stop();
        pBLEScan->clearResults();
        pBLEScan = nullptr;
    }

    if (is_ble_inited) {
#if !defined(LITE_VERSION)
        if (BLEStateManager::isBLEActive() || BLEStateManager::getActiveClientCount() > 0) {
            if (!BLEStateManager::deinitBLE(true)) return;
        } else
#endif
            if (BLEDevice::getScan() != nullptr || BLEDevice::getAdvertising() != nullptr ||
                BLEDevice::getServer() != nullptr || BLEConnected) {
            BLEDevice::deinit();
        }
    }

    pServer = nullptr;
    pService = nullptr;
    pTxCharacteristic = nullptr;
    pRxCharacteristic = nullptr;
    deviceConnected = false;
    oldDeviceConnected = false;
    bleDataTransferEnabled = false;
    is_ble_inited = false;
    BLEConnected = false;
#if !defined(LITE_VERSION)
    if (hid_ble) {
        delete hid_ble;
        hid_ble = nullptr;
    }
#endif
}

bool ble_scan_setup() {
    if (FORCE_RADIO_TEARDOWN_ON_SWITCH) {
        if (WiFi.getMode() != WIFI_MODE_NULL || wifiConnected) {
            wifiDisconnect();
            delay(200);
        }

        stopBLEStack();
        delay(100);
    }

    RAM_LOG("ble-scan pre-init");

    // FIX: Always try to init - if already init'd, it's a no-op
    if (!radioHasMemForBle()) {
        displayError("Low RAM: free WiFi/SD first", true);
        returnToMenu = true;
        return false;
    }

    BLEDevice::init("");
    is_ble_inited = true;

    RAM_LOG("ble-scan post-init");
    pBLEScan = BLEDevice::getScan();
    if (!pBLEScan) {
        displayError("Failed to get scan object", true);
        return false;
    }

    if (pBLEScan->isScanning()) {
        displayError("BLE scanner busy");
        return false;
    }

    pBLEScan->setScanCallbacks(&g_scanCallbacks);
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(SCAN_INT);
    pBLEScan->setWindow(SCAN_WINDOW);
    pBLEScan->setDuplicateFilter(false);

    esp_read_mac(sta_mac, ESP_MAC_BT);

    sprintf(
        strID,
        "%02X:%02X:%02X:%02X:%02X:%02X",
        sta_mac[0],
        sta_mac[1],
        sta_mac[2],
        sta_mac[3],
        sta_mac[4],
        sta_mac[5]
    );
    vTaskDelay(100 / portTICK_PERIOD_MS);
    return true;
}

void ble_scan() {
    displayTextLine("Scanning..");

    options = {};
    options.reserve(MAX_DISPLAY_DEVICES);

    if (!ble_scan_setup() || pBLEScan == nullptr) {
        displayError("Failed to init BLE scan");
        return;
    }

    pBLEScan->clearResults();

    try {
        if (!bleScanCancellable(pBLEScan, scanTime * 1000)) return;
        BLEScanResults foundDevices = pBLEScan->getResults();
        int deviceCount = foundDevices.getCount();
        int processedCount = 0;
        int maxToProcess = min(deviceCount, MAX_DISPLAY_DEVICES);

        for (int i = 0; i < maxToProcess && processedCount < MAX_DISPLAY_DEVICES; i++) {
            const NimBLEAdvertisedDevice *advertisedDevice = foundDevices.getDevice(i);
            if (!advertisedDevice) continue;

            String bt_title = "";
            String bt_name;
            String bt_address;
            String bt_signal;

            bt_name = advertisedDevice->getName().c_str();
            bt_address = advertisedDevice->getAddress().toString().c_str();
            bt_signal = String(advertisedDevice->getRSSI());

            if (bt_name.isEmpty()) bt_name = "<no name>";
            else bt_title = bt_name;
            if (bt_title.isEmpty()) bt_title = bt_address;

            if (options.size() < MAX_DISPLAY_DEVICES) {
                options.emplace_back(bt_title.c_str(), [=]() { ble_info(bt_name, bt_address, bt_signal); });
#ifdef FLUX_DELTA_BUILD
                options.back().detail = bt_address;
                options.back().uiMetric = int16_t(advertisedDevice->getRSSI());
                options.back().uiKind = 2;
#endif
                processedCount++;
            }
        }

        if (options.size() >= MAX_DISPLAY_DEVICES) { options.emplace_back("... and more devices", nullptr); }
    } catch (...) {
        displayError("BLE scan error");
        pBLEScan->clearResults();
        return;
    }

    if (pBLEScan) { pBLEScan->stop(); }

    pBLEScan->clearResults();

    if (!options.empty()) {
        addOptionToMainMenu();
        loopOptions(options);
        options.clear();
    } else {
        displayError("No devices found");
        delay(1000);
    }
}

bool initBLEServer() {
    uint64_t chipid = ESP.getEfuseMac();
    String blename = "Flux-" + String((uint8_t)(chipid >> 32), HEX);

    if (!is_ble_inited) {
        BLEDevice::init(blename.c_str());
        is_ble_inited = true;
    }

    pServer = BLEDevice::createServer();
    if (!pServer) {
        displayError("Failed to create BLE server");
        return false;
    }

    pServer->setCallbacks(new MyServerCallbacks());
    pService = pServer->createService(SERVICE_UUID);
    if (!pService) {
        displayError("Failed to create BLE service");
        return false;
    }

    pTxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_RX_UUID, NIMBLE_PROPERTY::NOTIFY);
    if (!pTxCharacteristic) {
        displayError("Failed to create TX characteristic");
        return false;
    }

    pTxCharacteristic->addDescriptor(new NimBLE2904());
    pRxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_TX_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
    );
    if (!pRxCharacteristic) {
        displayError("Failed to create RX characteristic");
        return false;
    }
    pRxCharacteristic->setCallbacks(new MyCallbacks());

    return true;
}

void disPlayBLESend() {
    uint8_t senddata[2] = {0};
    tft.fillScreen(fluxConfig.bgColor);
    drawMainBorder();
    tft.setTextSize(1);

    if (!pServer) {
        if (!initBLEServer()) {
            displayError("Failed to init BLE server");
            return;
        }
    }

    pServer->getAdvertising()->start();

    uint64_t chipid = ESP.getEfuseMac();
    String blename = "Flux-" + String((uint8_t)(chipid >> 32), HEX);

    BLEConnected = true;

    bool wasConnected = false;
    bool first_run = true;
    while (!check(EscPress)) {
        if (deviceConnected) {
            if (!wasConnected) {
                tft.fillRect(10, 26, tftWidth - 20, tftHeight - 36, TFT_BLACK);
                drawBLE_beacon(180, 28, TFT_BLUE);
                tft.setTextColor(fluxConfig.priColor, fluxConfig.bgColor);
                tft.setTextSize(FM);
                tft.setCursor(12, 50);
                tft.printf("BLE Send\n");
                tft.setTextSize(FM);
            }
            tft.fillRect(10, 100, tftWidth - 20, 28, TFT_BLACK);
            tft.setCursor(12, 100);
            if (senddata[0] % 4 == 0) {
                tft.printf("0x%02X>    ", senddata[0]);
            } else if (senddata[0] % 4 == 1) {
                tft.printf("0x%02X>>   ", senddata[0]);
            } else if (senddata[0] % 4 == 2) {
                tft.printf("0x%02X >>  ", senddata[0]);
            } else if (senddata[0] % 4 == 3) {
                tft.printf("0x%02X  >  ", senddata[0]);
            }

            senddata[1]++;
            if (senddata[1] > 3) {
                senddata[1] = 0;
                senddata[0]++;
                pTxCharacteristic->setValue(senddata, 1);
                pTxCharacteristic->notify();
            }
            wasConnected = true;
        } else {
            if (wasConnected or first_run) {
                first_run = false;
                tft.fillRect(10, 26, tftWidth - 20, tftHeight - 36, TFT_BLACK);
                tft.setTextSize(FM);
                tft.setCursor(12, 50);
                tft.setTextColor(TFT_RED);
                tft.printf("BLE disconnect\n");
                tft.setCursor(12, 75);
                tft.setTextColor(tft.color565(18, 150, 219));

                tft.printf(String("Name:" + blename + "\n").c_str());
                tft.setCursor(12, 100);
                tft.printf("UUID:1bc68b2a\n");
                drawBLE_beacon(180, 40, TFT_DARKGREY);
            }
            wasConnected = false;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    tft.setTextColor(TFT_WHITE);
    pServer->getAdvertising()->stop();
    BLEConnected = false;
}

void ble_test() {
    printf("ble test\n");

    if (!is_ble_inited) {
        printf("Init ble server\n");
        if (!initBLEServer()) {
            displayError("Failed to init BLE server");
            return;
        }
        delay(100);
    }

    disPlayBLESend();

    if (pServer) { pServer->getAdvertising()->stop(); }
    stopBLEStack();

    printf("Quit ble test\n");
}
