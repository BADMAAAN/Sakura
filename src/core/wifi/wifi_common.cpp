#include "core/wifi/wifi_common.h"
#include "core/display.h"
#include "core/mykeyboard.h"
#include "core/powerSave.h"
#include "core/radio_mem.h"
#include "core/ram_profile.h"
#include "core/settings.h"
#include "core/utils.h"
#include "core/wifi/wifi_mac.h"
#include "esp_wifi.h"
#include "modules/ble/ble_common.h"
#include <esp_event.h>
#include <esp_netif.h>
#include <globals.h>
#include <atomic>

static TaskHandle_t timezoneTaskHandle = NULL;
static bool wifiTransitioning = false;
static std::atomic_flag wifiOperation = ATOMIC_FLAG_INIT;
static std::atomic<uint32_t> wifiScanDoneSequence{0};

struct WifiOperationGuard {
    bool acquired;
    WifiOperationGuard() : acquired(!wifiOperation.test_and_set()) {}
    ~WifiOperationGuard() { if (acquired) wifiOperation.clear(); }
};

static bool wifiBack(bool foreground) { return foreground && (check(EscPress) || returnToMenu); }
static bool wifiForeignOperation() {
    return wifiTransitioning || isWebUIActive || (WiFi.getMode() & WIFI_MODE_AP) ||
           (WiFi.getStatusBits() & WIFI_SCANNING_BIT);
}

static bool ensureOwnedWifiEvents() {
    static bool installed = false;
    if (!installed) installed = WiFi.onEvent([](arduino_event_id_t) { ++wifiScanDoneSequence; }, ARDUINO_EVENT_WIFI_SCAN_DONE) != 0;
    return installed;
}

static void stopOwnedWifiAttempt(bool foreground) {
    if (foreground) displayRedStripe("Cancelling WiFi...");
    bool accepted = WiFi.disconnect(false, false, 0);
    if (!accepted && foreground) displayRedStripe("Disconnect failed; stopping owned STA...");
    // Failed/unassociated attempts need not emit DISCONNECTED. Checked STA disable is the stop proof.
    // enableSTA(false) clears only STA, preserving any AP bit; no configuration is erased.
    bool stopped = WiFi.enableSTA(false);
    if ((!stopped || (WiFi.getMode() & WIFI_MODE_STA)) && foreground)
        displayRedStripe("WiFi stop failed; retrying owned STA stop...");
    uint32_t retryAt = millis();
    while (!stopped || (WiFi.getMode() & WIFI_MODE_STA)) {
        vTaskDelay(pdMS_TO_TICKS(20));
        if (millis() - retryAt >= 250) {
            stopped = WiFi.enableSTA(false);
            retryAt = millis();
        }
    }
    if (!(WiFi.getMode() & WIFI_MODE_AP)) wifiConnected = false;
}

static int scanOwnedWifi(bool foreground, WifiConnectOutcome &outcome) {
    if (wifiBack(foreground)) { outcome = WifiConnectOutcome::Cancelled; return -1; }
    if (WiFi.getStatusBits() & WIFI_SCANNING_BIT) { outcome = WifiConnectOutcome::Busy; return -1; }
    if (!ensureOwnedWifiEvents()) { outcome = WifiConnectOutcome::Failed; return -1; }
    uint32_t sequence = wifiScanDoneSequence.load();
    if (WiFi.scanNetworks(true) != WIFI_SCAN_RUNNING) { outcome = WifiConnectOutcome::Failed; return -1; }
    uint32_t started = millis();
    while (true) {
        // Back wins even if the completion event arrived in the same foreground tick.
        bool cancelled = wifiBack(foreground);
        bool done = wifiScanDoneSequence.load() != sequence;
        if (cancelled || millis() - started >= 15000) {
            outcome = cancelled ? WifiConnectOutcome::Cancelled : WifiConnectOutcome::Failed;
            if (foreground) displayRedStripe(cancelled ? "Cancelling WiFi scan..." : "Stopping timed out scan...");
            if (!done) {
                esp_err_t stopped = esp_wifi_scan_stop();
                if (stopped != ESP_OK && foreground) displayRedStripe("Scan stop failed; waiting for completion");
                // scanComplete's timeout clears only wrapper flags; only the SDK event releases ownership.
                while (wifiScanDoneSequence.load() == sequence) { vTaskDelay(pdMS_TO_TICKS(20)); }
            }
            WiFi.scanDelete();
            return -1;
        }
        if (done) return WiFi.scanComplete();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

esp_err_t wifiRawTx(wifi_interface_t ifx, const void *frame, int len, uint8_t retries) {
    esp_err_t err = esp_wifi_80211_tx(ifx, frame, len, false);
    for (uint8_t i = 0; err == ESP_ERR_NO_MEM && i < retries; i++) {
        vTaskDelay(1); // let the driver drain TX buffers and retry
        err = esp_wifi_80211_tx(ifx, frame, len, false);
    }
    return err;
}

void ensureWifiPlatform() {
    static bool netifInitialized = false;
    static bool eventLoopCreated = false;
    static portMUX_TYPE platformMux = portMUX_INITIALIZER_UNLOCKED;

    portENTER_CRITICAL(&platformMux);
    bool needNetif = !netifInitialized;
    bool needLoop = !eventLoopCreated;
    portEXIT_CRITICAL(&platformMux);

    if (needNetif) {
        ESP_ERROR_CHECK(esp_netif_init());
        portENTER_CRITICAL(&platformMux);
        netifInitialized = true;
        portEXIT_CRITICAL(&platformMux);
    }

    if (needLoop) {
        esp_err_t err = esp_event_loop_create_default();
        if (err != ESP_ERR_INVALID_STATE) { ESP_ERROR_CHECK(err); }
        portENTER_CRITICAL(&platformMux);
        eventLoopCreated = true;
        portEXIT_CRITICAL(&platformMux);
    }
}

bool _wifiConnect(const String &ssid, int encryption, WifiConnectOutcome *outcome) {
    WifiConnectOutcome result = WifiConnectOutcome::Failed;
    auto finish = [&](WifiConnectOutcome value) { if (outcome) *outcome = value; return value == WifiConnectOutcome::Connected; };
    if (wifiBack(true)) return finish(WifiConnectOutcome::Cancelled);
    String password = fluxConfig.getWifiPassword(ssid);
    if (password == "" && encryption > 0) { password = keyboard(password, 63, "Network Password:", true); }
    if (password == "\x1B" || wifiBack(true)) return finish(WifiConnectOutcome::Cancelled);
    bool connected = _connectToWifiNetwork(ssid, password, &result);
    bool retry = false;

    while (!connected) {
        if (result == WifiConnectOutcome::Cancelled || result == WifiConnectOutcome::Busy) return finish(result);
        wakeUpScreen();

        options = {
            {"Retry",  [&]() { retry = true; } },
            {"Cancel", [&]() { retry = false; }},
        };
        int selected = loopOptions(options);

        if (selected < 0 || !retry || wifiBack(true)) return finish(WifiConnectOutcome::Cancelled);

        password = keyboard(password, 63, "Network Password:", true);
        if (password == "\x1B" || wifiBack(true)) return finish(WifiConnectOutcome::Cancelled);
        connected = _connectToWifiNetwork(ssid, password, &result);
    }

    if (connected) {
        if (wifiBack(true)) { stopOwnedWifiAttempt(true); return finish(WifiConnectOutcome::Cancelled); }
        wifiConnected = true;
        wifiIP = WiFi.localIP().toString();
        fluxConfig.addWifiCredential(ssid, password);

        // Start timezone update in background if not already running
        if (timezoneTaskHandle == NULL) {
            xTaskCreate(updateTimezoneTask, "updateTimezone", 4096, NULL, 1, &timezoneTaskHandle);
        }
    }

    return finish(connected ? WifiConnectOutcome::Connected : result);
}

bool _connectToWifiNetwork(const String &ssid, const String &pwd, WifiConnectOutcome *outcome, bool foreground) {
    auto finish = [&](WifiConnectOutcome value) { if (outcome) *outcome = value; return value == WifiConnectOutcome::Connected; };
    if (wifiBack(foreground)) return finish(WifiConnectOutcome::Cancelled);
    if (wifiForeignOperation() || WiFi.isConnected()) return finish(WifiConnectOutcome::Busy);
    if (FORCE_RADIO_TEARDOWN_ON_SWITCH) {
        if (BLEConnected) {
            if (foreground) displayWarning("Board with no PSRAM, closing BLE Stack");
        }
        stopBLEStack();
    }

    RAM_LOG("wifi pre-mode"); // Wi-Fi is already up from the menu scan by this point
    if (wifiBack(foreground)) return finish(WifiConnectOutcome::Cancelled);
    if (foreground) {
        drawMainBorderWithTitle("WiFi Connect");
        padprintln("");
        padprint("Connecting to: " + ssid + ".");
    }
    if (!WiFi.mode(WIFI_MODE_STA)) return finish(WifiConnectOutcome::Failed);
    RAM_LOG("wifi post-mode");
    if (wifiBack(foreground)) return finish(WifiConnectOutcome::Cancelled);
    WiFi.begin(ssid, pwd);
    uint32_t started = millis();
    while (true) {
        if (wifiBack(foreground)) {
            stopOwnedWifiAttempt(foreground);
            return finish(WifiConnectOutcome::Cancelled);
        }
        if (WiFi.isConnected()) return finish(WifiConnectOutcome::Connected);
        if (millis() - started >= 10000) {
            stopOwnedWifiAttempt(foreground);
            if (foreground) displayError("Wifi Offline", false);
            return finish(WifiConnectOutcome::Failed);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

bool _setupAP() {
    IPAddress AP_GATEWAY(172, 0, 0, 1);
    WiFi.softAPConfig(AP_GATEWAY, AP_GATEWAY, IPAddress(255, 255, 255, 0));
    WiFi.softAP(fluxConfig.wifiAp.ssid, fluxConfig.wifiAp.pwd, 6, 0, 4, false);
    wifiIP = WiFi.softAPIP().toString(); // update global var
    Serial.println("IP: " + wifiIP);
    wifiConnected = true;
    return true;
}

void wifiDisconnect() {
    wifiTransitioning = true;

    wifi_mode_t mode = WiFi.getMode();
    if (mode & WIFI_MODE_AP) {
        WiFi.softAPdisconnect();
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    if (mode & WIFI_MODE_STA) {
        WiFi.disconnect(false, true);
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    if (mode != WIFI_MODE_NULL) {
        WiFi.mode(WIFI_OFF);
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    wifiConnected = false;
    wifiTransitioning = false;
}

bool wifiConnectMenu(wifi_mode_t mode) {
    auto finish = [](WifiConnectOutcome value) { return value == WifiConnectOutcome::Connected; };
    WifiOperationGuard operation;
    if (!operation.acquired || wifiForeignOperation()) return finish(WifiConnectOutcome::Busy);
    if (wifiBack(true)) return finish(WifiConnectOutcome::Cancelled);
    if (WiFi.isConnected()) return false; // Preserve the existing public safeguard.
    WifiConnectOutcome result = WifiConnectOutcome::Failed;

    if (FORCE_RADIO_TEARDOWN_ON_SWITCH) {
        stopBLEStack();
    }

    // Check if WiFi is in transition
    if (wifiTransitioning) {
        displayTextLine("WiFi busy, please wait...");
        return finish(WifiConnectOutcome::Busy);
    }

    switch (mode) {
        case WIFI_AP: // access point
            if (wifiBack(true)) return finish(WifiConnectOutcome::Cancelled);
            if (!WiFi.mode(WIFI_AP)) return finish(WifiConnectOutcome::Failed);
            return finish(_setupAP() ? WifiConnectOutcome::Connected : WifiConnectOutcome::Failed);
            break;

        case WIFI_STA: { // station mode
            int nets;
            if (!radioHasMemForWifi()) {
                displayError("Low RAM: free BLE/SD first", true);
                return finish(WifiConnectOutcome::Failed);
            }
            if (wifiBack(true)) return finish(WifiConnectOutcome::Cancelled);
            if (!WiFi.mode(WIFI_MODE_STA)) return finish(WifiConnectOutcome::Failed);

            // wifiMACMenu();
            applyConfiguredMAC();

            bool refresh_scan = false;
            do {
                displayRedStripe("Scanning..");
                nets = scanOwnedWifi(true, result);
                if (nets < 0) return finish(result);

                String selSsid = "";
                int selEnc = 0;
                bool selHidden = false;

                options = {};
                for (int i = 0; i < nets; i++) {
                    if (options.size() < 250) {
                        String ssid = WiFi.SSID(i);
                        int encryptionType = WiFi.encryptionType(i);
                        int32_t rssi = WiFi.RSSI(i);
                        int32_t ch = WiFi.channel(i);
                        // Check if the network is secured
                        String encryptionPrefix = (encryptionType == WIFI_AUTH_OPEN) ? "" : "#";
                        String encryptionTypeStr;
                        switch (encryptionType) {
                            case WIFI_AUTH_OPEN: encryptionTypeStr = "Open"; break;
                            case WIFI_AUTH_WEP: encryptionTypeStr = "WEP"; break;
                            case WIFI_AUTH_WPA_PSK: encryptionTypeStr = "WPA/PSK"; break;
                            case WIFI_AUTH_WPA2_PSK: encryptionTypeStr = "WPA2/PSK"; break;
                            case WIFI_AUTH_WPA_WPA2_PSK: encryptionTypeStr = "WPA/WPA2/PSK"; break;
                            case WIFI_AUTH_WPA2_ENTERPRISE: encryptionTypeStr = "WPA2/Enterprise"; break;
                            case WIFI_AUTH_WPA3_PSK: encryptionTypeStr = "WPA3/PSK"; break;
                            case WIFI_AUTH_WPA2_WPA3_PSK: encryptionTypeStr = "WPA2/WPA3/PSK"; break;
                            default: encryptionTypeStr = "Unknown"; break;
                        }

                        String optionText = encryptionPrefix + ssid + "(" + String(rssi) + "|" +
                                            encryptionTypeStr + "|ch." + String(ch) + ")";

                        options.push_back({optionText.c_str(), [&selSsid, &selEnc, ssid, encryptionType]() {
                                               selSsid = ssid;
                                               selEnc = encryptionType;
                                           }});
#ifdef FLUX_DELTA_BUILD
                        // Keep real scan metadata after scanDelete(); handlers remain unchanged.
                        options.back().label = ssid.length() ? ssid : "<hidden SSID>";
                        options.back().detail = encryptionTypeStr + "  " + WiFi.BSSIDstr(i);
                        options.back().uiMetric = int16_t(rssi);
                        options.back().uiKind = 1;
                        options.back().uiChannel = uint8_t(ch);
#endif
                    }
                }
                WiFi.scanDelete();
                options.push_back({"Hidden SSID", [&selHidden]() { selHidden = true; }});
                addOptionToMainMenu();

                int selected = loopOptions(options);
                options.clear();

                if (selected < 0 || wifiBack(true)) {
                    return finish(WifiConnectOutcome::Cancelled);
                } else if (selHidden) {
                    String __ssid = keyboard("", 32, "Your SSID");
                    if (__ssid == "\x1B" || wifiBack(true)) return finish(WifiConnectOutcome::Cancelled);
                    _wifiConnect(__ssid.c_str(), 8, &result);
                    refresh_scan = false;
                } else if (selSsid != "") {
                    _wifiConnect(selSsid, selEnc, &result);
                    refresh_scan = false;
                } else {
                    refresh_scan = false;
                }
            } while (refresh_scan);
        } break;

        case WIFI_AP_STA: // repeater mode
                          // _setupRepeater();
            break;

        default: // error handling
            Serial.println("Unknown wifi mode: " + String(mode));
            break;
    }

    return finish(result);
}

static bool connectKnownWifi(bool foreground, WifiConnectOutcome *outcome) {
    auto finish = [&](WifiConnectOutcome value) {
        if (outcome) *outcome = value;
        return value == WifiConnectOutcome::Connected;
    };
    WifiOperationGuard operation;
    if (!operation.acquired || wifiForeignOperation()) return finish(WifiConnectOutcome::Busy);
    if (wifiBack(foreground)) return finish(WifiConnectOutcome::Cancelled);
    if (WiFi.isConnected()) return finish(WifiConnectOutcome::Connected);
    if (!radioHasMemForWifi()) {
        if (foreground) displayError("Low RAM: free BLE/SD first", true);
        return finish(WifiConnectOutcome::Failed);
    }
    if (FORCE_RADIO_TEARDOWN_ON_SWITCH) stopBLEStack();
    if (wifiBack(foreground)) return finish(WifiConnectOutcome::Cancelled);
    if (!WiFi.mode(WIFI_MODE_STA)) return finish(WifiConnectOutcome::Failed);
    if (foreground) displayRedStripe("Scanning Networks..");
    WifiConnectOutcome result = WifiConnectOutcome::Failed;
    int nets = scanOwnedWifi(foreground, result);
    if (nets < 0) return finish(result);
    std::vector<std::pair<String, String>> networks;
    for (int i = 0; i < nets; ++i) {
        String ssid = WiFi.SSID(i);
        String password = fluxConfig.getWifiPassword(ssid);
        if (password != "") networks.push_back({ssid, password});
    }
    WiFi.scanDelete(); // Results are complete and copied; this never stops a live scan.
    for (const auto &network : networks) {
        if (wifiBack(foreground)) return finish(WifiConnectOutcome::Cancelled);
        bool connected = _connectToWifiNetwork(network.first, network.second, &result, foreground);
        if (result == WifiConnectOutcome::Cancelled || result == WifiConnectOutcome::Busy) return finish(result);
        if (!connected) continue;
        if (wifiBack(foreground)) { stopOwnedWifiAttempt(foreground); return finish(WifiConnectOutcome::Cancelled); }
        wifiConnected = true;
        wifiIP = WiFi.localIP().toString();
        if (timezoneTaskHandle == NULL) xTaskCreate(updateTimezoneTask, "updateTimezone", 4096, NULL, 1, &timezoneTaskHandle);
        return finish(WifiConnectOutcome::Connected);
    }
    return finish(result);
}

void wifiConnectTask(void *pvParameters) {
    connectKnownWifi(false, nullptr); // Background auto-connect never reads foreground Back.
    vTaskDelete(NULL);
}

String checkMAC() { return String(WiFi.macAddress()); }

bool wifiConnecttoKnownNet(WifiConnectOutcome *outcome) {
    return connectKnownWifi(true, outcome);
}

void updateTimezoneTask(void *pvParameters) {
    // Wait a bit for connection to stabilize before updating timezone
    vTaskDelay(5000 / portTICK_PERIOD_MS);

    // Only update timezone if WiFi is still connected
    if (WiFi.isConnected() && wifiConnected) { updateClockTimezone(); }

    // Clear the task handle before deleting
    timezoneTaskHandle = NULL;
    vTaskDelete(NULL);
}
