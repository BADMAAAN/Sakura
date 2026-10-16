#include "delta_ui.h"
#ifdef FLUX_DELTA_BUILD
#include "display.h"
#include "main_menu.h"
#include "powerSave.h"
#include "settings.h"
#ifdef SAKURA_BUILD
#include "sakura_navigation.h"
#endif
#include <delta/core.h>
#include <delta/settings.h>
#include <delta/instruments.h>
#include <delta/section_ui.h>
#include <delta/ble_surface.h>
#include <delta/dirty.h>
#include <esp_heap_caps.h>
#include <interface.h>
#include <Preferences.h>
#include <string.h>
#include <algorithm>

namespace {
delta::Settings settings;
delta::Appearance appearance;
delta::Navigation navigation;
delta::Status status;
uint16_t *frame = nullptr, *previousFrame = nullptr, *backgroundFrame = nullptr;
bool previousValid = false, retainedCanvas = false, benchmarkControl = false;
int backgroundKind = -1;
uint32_t lastPresentBytes = 0;
uint32_t reduceDecorationUntil = 0;
volatile bool benchmarkRequested = false;
volatile uint32_t physicalInputAt = 0;
uint32_t consumedInputAt = 0, pendingInputAt = 0, inputQuietAt = 0;
uint32_t inputSamples[32]; unsigned inputCount = 0;
void serviceBenchmark();
void prepareBackground(bool home) {
    if (!backgroundFrame) return;
    const int kind = int(appearance.theme) + (home ? 3 : 0);
    if (backgroundKind == kind) return;
    auto& cache = delta::ui::backgroundCache(); cache.theme = -1;
    if (home) delta::renderBackground(backgroundFrame, appearance);
    else delta::ui::workBackground(backgroundFrame, appearance);
    backgroundKind = kind; cache.pixels = backgroundFrame; cache.theme = home ? -1 : int(appearance.theme);
}
void capturePhysicalInput() {
    const uint32_t sample = physicalInputAt;
    if (sample && sample != consumedInputAt) { consumedInputAt = sample; pendingInputAt = sample; }
}
void flushInputMeasurements() {
    if (!inputCount || (inputCount < 32 && millis() - inputQuietAt < 1500)) return;
    std::sort(inputSamples, inputSamples + inputCount);
    Serial.printf("[DELTA INPUT] n=%u min_us=%u median_us=%u p95_us=%u max_us=%u\n", inputCount, unsigned(inputSamples[0]), unsigned(inputSamples[inputCount/2]), unsigned(inputSamples[(inputCount*95-1)/100]), unsigned(inputSamples[inputCount-1]));
    inputCount = 0;
}
char currentLabel[32] = "Wi-Fi", previousLabel[32] = "Wi-Fi", clockText[6] = "--:--";
delta::Icon currentIcon = delta::Icon::Wifi, previousIcon = delta::Icon::Wifi;
bool initialized = false, dirty = true, wasOff = false, allocationWarned = false;
uint32_t lastFrameAt = 0, lastStatusAt = 0, measuredAt = 0, frames = 0;
uint64_t renderUs = 0, transferUs = 0;
struct WorkRow {
    String label, value;
    bool enabled = true, checked = false;
    uint8_t flags = 0, kind = 0, channel = 0;
    int16_t metric = -32768;
};
// Owned text survives nested menu vectors and callbacks. Capacity is reused; animation never allocates.
std::vector<WorkRow> workRows;
std::vector<delta::ui::MenuRow> workView;
std::vector<delta::ui::RadioRow> radioView;
String workTitle;
int workSelected = 0, workFirst = 0, workOldY = 39;
uint32_t workChangedAt = 0, workFrameAt = 0;
uint32_t workOpenedAt = 0, homeOpenedAt = 0;
bool workActive = false;
const char *const sectionNames[] = {
    "WiFi", "BLE", "RF", "LoRa", "NRF24", "FM", "IR", "Ethernet", "GPS", "RFID", "Files",
    "JS Interpreter", "Clock", "Others", "Config", "Favorites"
};

int sectionId(const String &name) {
    for (unsigned i = 0; i < sizeof(sectionNames) / sizeof(sectionNames[0]); ++i)
        if (name == sectionNames[i]) return i;
    return -1;
}
delta::Icon sectionIcon(int id) {
    static const delta::Icon icons[] = {
        delta::Icon::Wifi, delta::Icon::Bluetooth, delta::Icon::Radio, delta::Icon::LoRa,
        delta::Icon::Radio, delta::Icon::Radio, delta::Icon::Infrared, delta::Icon::Ethernet,
        delta::Icon::Radio, delta::Icon::Rfid, delta::Icon::Folder, delta::Icon::Script,
        delta::Icon::Clock, delta::Icon::Tools, delta::Icon::Settings, delta::Icon::Favorites
    };
    return id >= 0 && id < 16 ? icons[id] : delta::Icon::Tools;
}
const char *sectionLabel(int id, const String &fallback) {
    if (id == 0) return "Wi-Fi";
    if (id == 11) return "Scripts";
    if (id == 13) return "Tools";
    if (id == 14) return "Settings";
    return fallback.c_str();
}
void applyAppearance() {
    appearance.theme = static_cast<delta::Theme>(settings.theme);
    appearance.quality = settings.quality;
    appearance.petals = settings.petals;
    appearance.transitionMs = settings.transitionMs;
    appearance.glitch = settings.glitch;
    appearance.reducedMotion = settings.reducedMotion;
    appearance.status = settings.status;
    appearance.icons = settings.icons;
    backgroundKind = -1; delta::ui::backgroundCache().theme = -1; previousValid = false;
    dirty = true;
}
void initialize() {
    if (initialized) return;
    initialized = true;
    Preferences prefs;
    // Missing namespace, corrupt records and unknown versions use the embedded default without boot writes.
    if (prefs.begin("flux_delta", true)) {
        uint8_t data[delta::SettingsBytes];
        size_t count = prefs.getBytesLength("appearance");
        if (count <= sizeof(data) && prefs.getBytes("appearance", data, count) == count)
            delta::decodeSettings(data, count, settings);
        prefs.end();
    }
    applyAppearance();
    frame = static_cast<uint16_t *>(heap_caps_malloc(delta::Width * delta::Height * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    previousFrame = static_cast<uint16_t *>(heap_caps_malloc(delta::Width * delta::Height * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    backgroundFrame = static_cast<uint16_t *>(heap_caps_malloc(delta::Width * delta::Height * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if(frame)memset(frame,0,delta::Width*delta::Height*sizeof(uint16_t));
    measuredAt = millis();
    Serial.printf("[DELTA] framebuffer=%s bytes=%u settings=v2\n", frame ? "PSRAM" : "unavailable", unsigned(delta::Width * delta::Height * sizeof(uint16_t)));
}
bool saveSettings() {
    uint8_t data[delta::SettingsBytes];
    delta::encodeSettings(settings, data);
    Preferences prefs;
    bool saved = false;
    if (prefs.begin("flux_delta", false)) {
        saved = prefs.putBytes("appearance", data, sizeof(data)) == sizeof(data);
        prefs.end();
    }
    applyAppearance();
    if (!saved) displayError("Sakura settings not saved", true);
    return saved;
}
void refreshStatus(uint32_t now) {
    if (now - lastStatusAt < 1000 && lastStatusAt != 0) return;
    lastStatusAt = now;
    status.battery = getBattery();
    status.charging = isCharging();
    status.wifi = wifiConnected;
    status.ble = BLEConnected;
    struct tm time = rtc.getTimeStruct();
    snprintf(clockText, sizeof(clockText), "%02d:%02d", time.tm_hour, time.tm_min);
    status.time = clock_set ? clockText : "--:--";
    dirty = true;
}
bool enabled(MenuItemInterface *item) {
    const auto &disabled = fluxConfig.disabledMenus;
    return std::find(disabled.begin(), disabled.end(), item->getName()) == disabled.end();
}
void editFavorites() {
#ifdef SAKURA_BUILD
    sakuraEditFavoritesMenu();
#else
    int index = 0;
    while (!returnToMenu) {
        std::vector<Option> choices;
        for (auto *item : mainMenu.getItems()) {
            const int id = sectionId(item->getName());
            if (id < 0 || !enabled(item)) continue;
            const uint32_t bit = uint32_t(1) << id;
            const bool selected = settings.favorites & bit;
            choices.push_back({String(selected ? "Remove: " : "Add: ") + item->getName(), [bit]() {
                settings.favorites ^= bit;
                saveSettings();
            }, selected});
        }
        choices.push_back({"Back", []() {}});
        index = loopOptions(choices, MENU_TYPE_SUBMENU, "Edit Favorites", index);
        if (index < 0 || index == int(choices.size()) - 1) return;
    }
#endif
}
void consumeAmbientInput() {
#ifdef HAS_ENCODER
    DiscardEncoderInput = true;
#endif
    clearInputState();
#if defined(SEL_BTN) && defined(BTN_ACT)
    while (digitalRead(SEL_BTN) == BTN_ACT
#if defined(BK_BTN)
           || digitalRead(BK_BTN) == BTN_ACT
#endif
    ) vTaskDelay(pdMS_TO_TICKS(5));
#endif
    clearInputState();
#ifdef HAS_ENCODER
    DiscardEncoderInput = false;
#endif
}
void ambientScreen() {
    if (!frame || tftWidth != delta::Width || tftHeight != delta::Height) {
        displayError("Ambient needs 320x170 + PSRAM", true);
        return;
    }
    // This is an awake screen. The existing idle policy may still turn the panel off.
    dirty = true;
    deltaUiTick(true);
    consumeAmbientInput();
    while (true) {
        if (AnyKeyPress || SelPress || EscPress || NextPress || PrevPress || RotaryNetSteps) {
            consumeAmbientInput();
            previousMillis = millis();
            dirty = true;
            return;
        }
        deltaUiTick(true);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void renderWork(uint32_t now) {
    if (!frame || isScreenOff || isSleeping) return;
    refreshStatus(now);
    prepareBackground(false); retainedCanvas = true;
    const bool instrument = true;
    const int targetY = 39 + (workSelected - workFirst) * (instrument ? 36 : 25);
    const float t = appearance.reducedMotion || !appearance.transitionMs ? 1.f : std::min(1.f, float(now - workChangedAt) / appearance.transitionMs);
    const int focusY = workOldY + int((targetY - workOldY) * delta::smooth(t));
    if (workTitle == "WiFi" && radioView.empty()) {
        delta::ui::wifiMain(frame, appearance, workView.data(), workView.size(), workSelected, &status,
                           wifiConnected ? wifiIP.c_str() : nullptr, 39 + int(4 * (1.f - delta::smooth(t))));
    } else if (!radioView.empty() && workRows[0].kind) {
        delta::ui::radioResults(frame, appearance, workTitle.c_str(), radioView.data(), radioView.size(), workSelected, workRows[0].kind, &status, focusY);
    } else {
        const char* title = workTitle.c_str(); const char* subtitle = "";
        if (workTitle == "BLE" || workTitle == "Bluetooth") { title="BLUETOOTH"; subtitle="BLUETOOTH TOOLS"; }
        else if (workTitle == "RF" || workTitle.startsWith("Radio Frequency")) { title="RF CONTROL"; subtitle=workTitle.c_str(); }
        else if (workTitle == "Config" || workTitle == "Settings") { title="SETTINGS"; subtitle="DEVICE CONFIGURATION"; }
        else if (workTitle == "Sakura Appearance") { title="APPEARANCE"; subtitle="SAKURA NIGHT"; }
        else if (workTitle == "Files") { title="FILES"; subtitle="STORAGE"; }
        else if (workTitle == "Scripts" || workTitle == "JS Interpreter") { title="SCRIPTS"; subtitle="SCRIPT TOOLS"; }
        delta::ui::modeList(frame, appearance, title, subtitle, workView.data(), workView.size(), workSelected, &status, 39 + int(4 * (1.f - delta::smooth(t))));
    }
    // Motion stays in the focus/foreground: no full-screen alpha fade or overdraw.
    deltaUiPresent();
    workFrameAt = now;
}
} // namespace

uint16_t *deltaUiCanvas(bool clear, bool decoration, bool retained) {
    initialize();
    workActive = false; retainedCanvas = retained;
    if (!retained) previousValid = false;
    if (!frame || tftWidth != delta::Width || tftHeight != delta::Height || isScreenOff || isSleeping) return nullptr;
    if (clear) { prepareBackground(false); delta::ui::workBackground(frame, appearance, decoration); }
    return frame;
}
const delta::Appearance &deltaUiAppearance() { initialize(); return appearance; }
const delta::Status &deltaUiStatus() { initialize(); refreshStatus(millis()); return status; }
void deltaUiPresent(int x, int y, int width, int height) {
    if (!frame || isScreenOff || isSleeping) return;
    const int right = std::min(delta::Width, x + width), bottom = std::min(delta::Height, y + height);
    x = std::max(0, x); y = std::max(0, y);
    if (right <= x || bottom <= y) return;
    const bool oldSwap = tft.getSwapBytes();
    tft.setSwapBytes(true); lastPresentBytes = 0;
    if (x == 0 && y == 0 && right == delta::Width && bottom == delta::Height && retainedCanvas && previousFrame && !benchmarkControl) {
        delta::presentChanged(frame, previousValid ? previousFrame : nullptr, [](int top, int rows) {
            tft.pushImage(0, top, delta::Width, rows, frame + top * delta::Width);
            memcpy(previousFrame + top * delta::Width, frame + top * delta::Width, rows * delta::Width * sizeof(uint16_t));
            lastPresentBytes += rows * delta::Width * sizeof(uint16_t);
        });
        previousValid = true;
    } else {
        if (x == 0 && right == delta::Width) tft.pushImage(0, y, delta::Width, bottom - y, frame + y * delta::Width);
        else for (int yy = y; yy < bottom; ++yy) tft.pushImage(x, yy, right - x, 1, frame + yy * delta::Width + x);
        lastPresentBytes = (right - x) * (bottom - y) * sizeof(uint16_t);
        previousValid = false; // Untracked/legacy owner: the next retained page starts cold.
    }
    tft.setSwapBytes(oldSwap);
    if (pendingInputAt) {
        if (inputCount < 32) inputSamples[inputCount++] = uint32_t(micros() - pendingInputAt);
        pendingInputAt = 0; inputQuietAt = millis();
    }
}

bool deltaUiMenu(const char *title, const std::vector<Option> &items, int selected) {
    initialize();
    if (!frame || tftWidth != delta::Width || tftHeight != delta::Height) return false;
    const String nextTitle = title && *title ? title : currentLabel;
    const bool samePage = workActive && workTitle == nextTitle;
    const int oldTarget = 39 + (workSelected - workFirst) * 36;
    const float oldProgress = appearance.reducedMotion || !appearance.transitionMs ? 1.f : std::min(1.f, float(millis() - workChangedAt) / appearance.transitionMs);
    const int previousY = workOldY + int((oldTarget - workOldY) * delta::smooth(oldProgress));
    workRows.resize(items.size()); workView.resize(items.size()); radioView.resize(items.size());
    bool radio = false;
    for (size_t i = 0; i < items.size(); ++i) {
        const auto &source = items[i]; auto &row = workRows[i];
        row.label = source.label; row.value = source.detail; row.flags = 0;
        row.enabled = source.enabled; row.checked = source.selected;
        row.kind = source.uiKind; row.metric = source.uiMetric; row.channel = source.uiChannel;
        radio |= row.kind != 0;
        if (row.value.isEmpty()) {
            const int separator = row.label.indexOf(": ");
            if (separator > 0) {
                row.value = row.label.substring(separator + 2); row.label.remove(separator);
                if (row.value == "On" || row.value == "Off") row.flags = delta::ui::Toggle | (row.value == "On" ? delta::ui::Checked : 0);
            }
        }
        workView[i] = delta::ui::MenuRow(row.label.c_str(), row.value.c_str(), row.enabled, row.checked, row.flags);
        radioView[i] = {row.label.c_str(), row.value.c_str(), row.metric, row.channel};
    }
    if (!radio) radioView.clear();
    // A Back/Rescan row can precede radio results: retain the first actual result kind.
    if (radio) for (const auto &r : workRows) if (r.kind) { workRows[0].kind = r.kind; break; }
    workTitle = nextTitle;
    workSelected = delta::ui::bounded(selected, 0, std::max(0, int(items.size()) - 1));
    const bool instrument = true;
    workFirst = instrument ? delta::ui::radioFirst(items.size(), workSelected) : delta::ui::firstVisible(items.size(), workSelected);
    workOldY = samePage ? delta::ui::bounded(previousY, 39, instrument ? 111 : 139) : 39 + (workSelected - workFirst) * (instrument ? 36 : 25) + 5;
    workChangedAt = millis();
    if (!samePage) { workOpenedAt = workChangedAt; previousValid = false; }
    workActive = true;
    renderWork(workChangedAt);
    return true;
}
void deltaUiMenuTick() {
    if (!workActive || !frame || isScreenOff || isSleeping) return;
    flushInputMeasurements();
    const uint32_t now = millis();
    if (now - workFrameAt < 16) return;
    if (now - workChangedAt < appearance.transitionMs || now - workOpenedAt < appearance.transitionMs || now - lastStatusAt >= 1000) renderWork(now);
}
void deltaUiMenuEnd() { workActive = false; previousValid = false; retainedCanvas = false; pendingInputAt = 0; }
void deltaUiInputConsumed() { capturePhysicalInput(); }
void deltaUiInvalidate() { previousValid = false; dirty = true; lastStatusAt = 0; }
bool deltaUiRestore() {
    initialize();
    if(isScreenOff || isSleeping)return false;
    // Direct-drawn tools keep their ST7789 GRAM across SLPIN/SLPOUT. Their old
    // DELTA canvas is not the screen; menu callbacks revoke it before dispatch.
    if(!retainedCanvas)return false;
    if(!frame || tftWidth!=delta::Width || tftHeight!=delta::Height) {
        tft.native()->fillScreen(TFT_BLACK);
        return false;
    }
    previousValid=false;
    // pushImage is synchronous; the backlight stays dark until this complete frame is sent.
    deltaUiPresent();
    dirty=true;lastStatusAt=0;
    return true;
}
void deltaUiRequestBenchmark() { benchmarkRequested = true; }
void deltaUiPhysicalInput(uint32_t sampledAtUs) { physicalInputAt = sampledAtUs; }

namespace {
void benchmarkMemory(const char *stage) {
    const unsigned buffers = unsigned(frame != nullptr) + unsigned(previousFrame != nullptr) + unsigned(backgroundFrame != nullptr);
    Serial.printf("[DELTA MEM] stage=%s heap_free=%u heap_min=%u psram_free=%u framebuffer_buffers=%u framebuffer_bytes=%u allocation_count=unavailable\n",
                  stage, unsigned(ESP.getFreeHeap()), unsigned(ESP.getMinFreeHeap()), unsigned(ESP.getFreePsram()),
                  buffers, buffers * delta::Width * delta::Height * unsigned(sizeof(uint16_t)));
}
// Explicit opt-in diagnostic. It paints labelled fixtures; it never calls a module handler.
void benchmarkPaint(int scene, int selection) {
    delta::Status fixture; fixture.time = "BENCH";
    const delta::ui::MenuRow rows[] = {{"Appearance", "SAKURA NIGHT"}, {"Brightness", "75%"}, {"Motion", "Normal"}, {"Back"}};
    const delta::ui::RadioRow networks[] = {{"BENCH_AP_A", "WPA2", -62, 6}, {"BENCH_AP_B", "OPEN", -88, 11}, {"Back", "", -32768, 0}};
    const char* values[] = {"30 ms", "100 ms", "MAX", "Every Packet"};
    if (scene == 0) delta::ui::modeList(frame, appearance, "BENCH", "FIXTURE / NO RADIO", rows, 4, selection, &fixture);
    if (scene == 1) delta::ui::wifiMain(frame, appearance, rows, 4, selection, &fixture);
    if (scene == 2) delta::ui::radioResults(frame, appearance, "BENCH FIXTURE", networks, 3, selection, 1, &fixture);
    if (scene == 3) delta::ui::bleConfig(frame, appearance, "BENCH FIXTURE / NO RADIO", values, selection, -1, &fixture);
}
void serviceBenchmark() {
    if (!benchmarkRequested || !frame) return;
    benchmarkRequested = false;
    const uint32_t startInput = physicalInputAt;
    // Opt-in LCD diagnostics only: canonical RGB565, no radio or storage operations.
    const uint16_t colors[] = {0x0000, 0xffff, 0xf800, 0x07e0, 0x001f, 0xed17, 0xffe0, 0x07ff};
    for (int y = 0; y < delta::Height; ++y)
        for (int x = 0; x < delta::Width; ++x) frame[y * delta::Width + x] = colors[x / 40];
    previousValid = false; retainedCanvas = true; deltaUiPresent();
    Serial.println("[DELTA COLORS] black white red green blue pink yellow cyan");
    vTaskDelay(pdMS_TO_TICKS(1500));
    const char* scenes[] = {"settings", "wifi-main", "wifi-results", "ble-config"};
    prepareBackground(false);
    benchmarkMemory("begin");
    pendingInputAt = 0;
    for (int scene = 0; scene < 4; ++scene) for (int control = 1; control >= 0; --control) {
        benchmarkControl = control; delta::ui::backgroundCache().bypass = control;
        retainedCanvas = true; previousValid = false;
        benchmarkPaint(scene, 0); deltaUiPresent();
        for (int run = 0; run < 3; ++run) {
            uint32_t times[32], renderedUs = 0, presentedUs = 0, bytes = 0;
            for (int i = 0; i < 32; ++i) {
                if (physicalInputAt != startInput || SelPress || EscPress) goto finished;
                const uint32_t begin = micros(); benchmarkPaint(scene, 1 + i % 2);
                const uint32_t painted = micros(); deltaUiPresent(); const uint32_t end = micros();
                times[i] = end - begin; renderedUs += painted - begin; presentedUs += end - painted; bytes += lastPresentBytes;
                vTaskDelay(pdMS_TO_TICKS(1));
            }
            std::sort(times, times + 32);
            // With 32 frames, nearest-rank p99 is the maximum; preserve the old workload for comparison.
            Serial.printf("[DELTA BENCH] scene=%s mode=%s run=%d n=32 median_us=%u p95_us=%u p99_us=%u max_us=%u render_us=%u present_us=%u bytes=%u\n",
                          scenes[scene], control ? "full" : "cached-diff", run, unsigned(times[16]), unsigned(times[30]), unsigned(times[31]), unsigned(times[31]), unsigned(renderedUs / 32), unsigned(presentedUs / 32), unsigned(bytes / 32));
        }
    }
finished:
    benchmarkControl = false; delta::ui::backgroundCache().bypass = false;
    previousValid = false; retainedCanvas = true; dirty = true; backgroundKind = -1;
    benchmarkMemory("end");
    Serial.println("[DELTA BENCH] done");
}
} // namespace
uint32_t deltaUiMapColor(uint32_t value) {
    // Preserve semantic warning colours, raw images, graph samples and the TFT transparency sentinel.
    if (interpreter_state >= 2) return value; // Foreground and runtime.main callbacks own drawing colours.
    const auto c = delta::ui::palette(appearance.theme);
    if (value == 0x00ffffff) return value;
    if (value == fluxConfig.bgColor || value == TFT_BLACK) return c.background;
    if (value == TFT_WHITE) return c.text;
    if (value == fluxConfig.priColor) return c.accent;
    if (value == fluxConfig.secColor) return c.muted;
    return value;
}

bool deltaUiSelect(const String &name, int direction) {
    initialize();
    if (!frame || tftWidth != delta::Width || tftHeight != delta::Height) {
        if (!allocationWarned && !frame) {
            allocationWarned = true;
            Serial.println("[DELTA] PSRAM frame allocation failed; legacy menu fallback");
        }
        return false;
    }
    const int id = sectionId(name);
    const char *label = sectionLabel(id, name);
    if (strcmp(label, currentLabel) != 0) {
        memcpy(previousLabel, currentLabel, sizeof(previousLabel));
        previousIcon = currentIcon;
        strlcpy(currentLabel, label, sizeof(currentLabel));
        currentIcon = sectionIcon(id);
        navigation.select(id >= 0 ? id : 16, direction, millis());
    }
    dirty = true;
    return true;
}
void deltaUiEnter() {
    initialize();
    workActive = false;
    homeOpenedAt = millis(); previousValid = false; retainedCanvas = true;
    consumedInputAt = physicalInputAt; pendingInputAt = 0;
    navigation.previous = navigation.selected;
    dirty = true;
    lastFrameAt = millis() - 16;
    measuredAt = millis();
    frames = 0; renderUs = transferUs = 0;
}
void deltaUiTick(bool ambient) {
    if (!frame || tftWidth != delta::Width || tftHeight != delta::Height) return;
    if (isScreenOff || isSleeping) { wasOff = true; return; }
    serviceBenchmark(); flushInputMeasurements();
    const uint32_t now = millis();
    if (wasOff) { wasOff = false; previousValid = false; dirty = true; lastFrameAt = now - 16; }
    refreshStatus(now);
    const uint32_t interval = appearance.quality == 0 ? 33 : 16;
    const bool moving = !appearance.reducedMotion && (appearance.petals || appearance.glitch || now - navigation.changedAt < appearance.transitionMs || now - homeOpenedAt < appearance.transitionMs);
    if ((!pendingInputAt && now - lastFrameAt < interval) || (!dirty && !moving)) return;
    lastFrameAt = now;
    const uint32_t start = micros();
    prepareBackground(true); retainedCanvas = true;
    if (backgroundFrame) memcpy(frame, backgroundFrame, delta::Width * delta::Height * sizeof(uint16_t));
    else delta::renderBackground(frame, appearance);
    const auto activeAppearance = delta::adaptiveAppearance(appearance, reduceDecorationUntil && int32_t(reduceDecorationUntil - now) > 0);
    delta::renderOverlay(frame, activeAppearance, navigation, currentIcon, currentLabel, previousIcon, previousLabel, status, now, ambient);
    const uint32_t rendered = micros();
    deltaUiPresent();
    const uint32_t transferred = micros();
    if (transferred - start > interval * 1000 + 2000) reduceDecorationUntil = now + 1000;
    renderUs += uint32_t(rendered - start);
    transferUs += uint32_t(transferred - rendered);
    ++frames;
    dirty = false;
    if (now - measuredAt >= 5000) {
        Serial.printf("[DELTA FPS] frames=%u elapsed_ms=%u fps=%.2f render_us=%u transfer_us=%u battery=%d charging=%u\n", unsigned(frames), unsigned(now - measuredAt), float(frames) * 1000 / (now - measuredAt), unsigned(renderUs / frames), unsigned(transferUs / frames), status.battery, unsigned(status.charging));
        measuredAt = now; frames = 0; renderUs = transferUs = 0;
    }
}
void deltaFavoritesMenu() {
#ifdef SAKURA_BUILD
    sakuraFavoritesMenu();
#else
    initialize();
    std::vector<Option> choices;
    for (auto *item : mainMenu.getItems()) {
        const int id = sectionId(item->getName());
        if (id >= 0 && enabled(item) && (settings.favorites & (uint32_t(1) << id)))
            choices.push_back({item->getName(), [item]() { item->optionsMenu(); }});
    }
    choices.push_back({"Edit Favorites", editFavorites});
    choices.push_back({"Main Menu", []() {}});
    loopOptions(choices, MENU_TYPE_SUBMENU, "Favorites");
#endif
}
uint32_t deltaUiFavoriteSections() { initialize(); return settings.favorites; }
void deltaAppearanceMenu() {
    initialize();
    static const char *const themes[] = {"SAKURA NIGHT", "Sakura Dawn", "Monochrome"};
    static const char *const qualities[] = {"Low", "Normal", "High"};
    static const char *const petals[] = {"Off", "Low", "Normal", "High"};
    int index = 0;
    while (!returnToMenu) {
        std::vector<Option> choices = {
            {String("Theme: ") + themes[settings.theme], []() { settings.theme = (settings.theme + 1) % 3; saveSettings(); }},
            {String("Quality: ") + qualities[settings.quality], []() { settings.quality = (settings.quality + 1) % 3; saveSettings(); }},
            {String("Petals: ") + petals[settings.petals], []() { settings.petals = (settings.petals + 1) % 4; saveSettings(); }},
            {String("Transition: ") + settings.transitionMs + " ms", []() { settings.transitionMs = settings.transitionMs == 180 ? 140 : settings.transitionMs + 20; saveSettings(); }},
            {String("Subtle Glitch: ") + (settings.glitch ? "On" : "Off"), []() { settings.glitch = !settings.glitch; saveSettings(); }},
            {String("Reduced Motion: ") + (settings.reducedMotion ? "On" : "Off"), []() { settings.reducedMotion = !settings.reducedMotion; saveSettings(); }},
            {String("Status: ") + (settings.status ? "On" : "Off"), []() { settings.status = !settings.status; saveSettings(); }},
            {String("Icons: ") + (settings.icons ? "On" : "Off"), []() { settings.icons = !settings.icons; saveSettings(); }},
            {"Brightness", []() { setBrightnessMenu(); }},
            {"Favorites", deltaFavoritesMenu},
            {"Ambient Clock", ambientScreen},
            {"Back", []() {}}
        };
        index = loopOptions(choices, MENU_TYPE_SUBMENU, "Sakura Appearance", index);
        if (index < 0 || index == int(choices.size()) - 1) return;
    }
}
#endif
