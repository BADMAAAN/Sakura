#include "sakura_navigation.h"
#ifdef SAKURA_BUILD
#include "delta_ui.h"
#include "display.h"
#include "input_diagnostics.h"
#include "main_menu.h"
#include "modules/others/qrcode_menu.h"
#include "modules/wifi/ssh_profiles.h"
#include <Preferences.h>
#include <delta/quick_access.h>
#include <algorithm>
#include <cstring>

namespace {
delta::QuickAccess quick;
bool loaded = false;
const char *const sections[] = {
    "WiFi", "BLE", "RF", "LoRa", "NRF24", "FM", "IR", "Ethernet", "GPS", "RFID", "Files",
    "JS Interpreter", "Clock", "Others", "Config"
};
const char *const screens[] = {"", "Buttons", "Sakura Appearance", "QRCodes", "SSH Profiles"};
delta::QuickAction action(SakuraActionKind kind, uint32_t id) { return {uint8_t(kind), id}; }
void load() {
    if (loaded) return;
    loaded = true;
    Preferences prefs;
    bool restored = false;
    if (prefs.begin("sakura_quick", true)) {
        uint8_t bytes[delta::QuickBytes];
        const size_t count = prefs.getBytesLength("pins");
        if (count <= sizeof(bytes) && prefs.getBytes("pins", bytes, count) == count)
            restored = delta::decodeQuickAccess(bytes, count, quick);
        prefs.end();
    }
    if (!restored) {
        const uint32_t mask = deltaUiFavoriteSections();
        for (unsigned i = 0; i < 15; ++i) if (mask & (uint32_t(1) << i)) quick.pin({1, i}, true);
    }
}
bool save() {
    uint8_t bytes[delta::QuickBytes];
    const size_t count = delta::encodeQuickAccess(quick, bytes);
    Preferences prefs;
    if (!prefs.begin("sakura_quick", false)) return false;
    const bool result = prefs.putBytes("pins", bytes, count) == count;
    prefs.end();
    return result;
}
MenuItemInterface *section(uint32_t id) {
    if (id >= 15) return nullptr;
    // The inherited Scripts menu resumes an existing interpreter; quick access never does.
    if (id == 11 && interpreter_state >= 0) return nullptr;
    for (auto *item : mainMenu.getItems()) {
        if (item->getName() != sections[id]) continue;
        const auto &disabled = fluxConfig.disabledMenus;
        if (std::find(disabled.begin(), disabled.end(), item->getName()) == disabled.end()) return item;
    }
    return nullptr;
}
String label(delta::QuickAction target) {
    if (target.kind == 1) { auto *item = section(target.id); return item ? item->getName() : String(); }
    if (target.kind == 2 && target.id > 0 && target.id < sizeof(screens) / sizeof(*screens)) return screens[target.id];
    if (target.kind == 3) for (const auto &profile : sshProfileLinks())
        if (profile.id == target.id) return String("SSH: ") + profile.name;
    if (target.kind == 4) for (const auto &qr : savedQrLinks())
        if (qr.id == target.id) return String("QR: ") + qr.name;
    return "";
}
void open(delta::QuickAction target) {
    if (target.kind == 1) { if (auto *item = section(target.id)) { quick.record(target); item->optionsMenu(); } }
    if (target.kind == 2) {
        quick.record(target);
        if (target.id == 1) inputButtonsScreen();
        if (target.id == 2) deltaAppearanceMenu();
        if (target.id == 3) qrcode_menu();
        if (target.id == 4) sshProfilesMenu();
    }
    if (target.kind == 3) openSshProfile(target.id);
    if (target.kind == 4) openSavedQr(target.id);
}
void addSections() {
    for (;;) {
        std::vector<Option> choices;
        for (unsigned id = 0; id < 15; ++id) {
            auto *item = section(id);
            if (!item) continue;
            const bool pinned = quick.find({1, id}) >= 0;
            choices.push_back({String(pinned ? "Remove: " : "Pin: ") + item->getName(), [id, pinned]() {
                if (!sakuraSetPinned(SakuraActionKind::Section, id, !pinned)) displayError("Favorites not saved", true);
            }, pinned});
        }
        choices.push_back({"Back", []() {}});
        const int selected = loopOptions(choices, MENU_TYPE_SUBMENU, "Pin section");
        if (returnToMenu || selected < 0 || selected == int(choices.size()) - 1) return;
    }
}
void addScreens() {
    for (;;) {
        std::vector<Option> choices;
        for (unsigned id = 1; id < sizeof(screens) / sizeof(*screens); ++id) {
            const bool pinned = quick.find({2, id}) >= 0;
            choices.push_back({String(pinned ? "Remove: " : "Pin: ") + screens[id], [id, pinned]() {
                if (!sakuraSetPinned(SakuraActionKind::Screen, id, !pinned)) displayError("Favorites not saved", true);
            }, pinned});
        }
        choices.push_back({"Back", []() {}});
        const int selected = loopOptions(choices, MENU_TYPE_SUBMENU, "Pin screen");
        if (returnToMenu || selected < 0 || selected == int(choices.size()) - 1) return;
    }
}
void addSshProfiles() {
    for (;;) {
        std::vector<Option> choices;
        for (const auto &profile : sshProfileLinks()) {
            const bool pinned = quick.find({3, profile.id}) >= 0;
            choices.push_back({String(pinned ? "Remove: " : "Pin: ") + profile.name, [profile, pinned]() {
                if (!sakuraSetPinned(SakuraActionKind::Ssh, profile.id, !pinned)) displayError("Favorites not saved", true);
            }, pinned});
        }
        choices.push_back({"Back", []() {}});
        const int selected = loopOptions(choices, MENU_TYPE_SUBMENU, "Pin SSH profile");
        if (returnToMenu || selected < 0 || selected == int(choices.size()) - 1) return;
    }
}
void addQrCodes() {
    for (;;) {
        std::vector<Option> choices;
        for (const auto &qr : savedQrLinks()) {
            const bool pinned = quick.find({4, qr.id}) >= 0;
            choices.push_back({String(pinned ? "Remove: " : "Pin: ") + qr.name, [qr, pinned]() {
                if (!sakuraSetPinned(SakuraActionKind::Qr, qr.id, !pinned)) displayError("Favorites not saved", true);
            }, pinned});
        }
        choices.push_back({"Back", []() {}});
        const int selected = loopOptions(choices, MENU_TYPE_SUBMENU, "Pin QR");
        if (returnToMenu || selected < 0 || selected == int(choices.size()) - 1) return;
    }
}
void editPin(delta::QuickAction target) {
    for (;;) {
        const int position = quick.find(target);
        if (position < 0) return;
        std::vector<Option> choices = {
            {"Open", [target]() { open(target); }},
            {"Move up", [position]() {
                if (position == 0) return;
                const auto before = quick;
                std::swap(quick.pins[position], quick.pins[position - 1]);
                if (!save()) { quick = before; displayError("Favorites not saved", true); }
            }},
            {"Move down", [position]() {
                if (position + 1 >= quick.count) return;
                const auto before = quick;
                std::swap(quick.pins[position], quick.pins[position + 1]);
                if (!save()) { quick = before; displayError("Favorites not saved", true); }
            }},
            {"Remove", [target]() { sakuraForget(SakuraActionKind(target.kind), target.id); }},
            {"Back", []() {}}
        };
        const int selected = loopOptions(choices, MENU_TYPE_SUBMENU, "Favorite");
        if (returnToMenu || selected < 0 || selected == 3 || selected == 4) return;
    }
}
}

bool sakuraIsPinned(SakuraActionKind kind, uint32_t id) { load(); return quick.find(action(kind, id)) >= 0; }
bool sakuraSetPinned(SakuraActionKind kind, uint32_t id, bool pinned) {
    load();
    const auto target = action(kind, id);
    if (!delta::validQuickAction(target)) return false;
    if ((quick.find(target) >= 0) == pinned) return true;
    // Persist legacy QR identity before its reference enters the independent NVS store.
    if (kind == SakuraActionKind::Qr && pinned && !fluxConfig.saveQrFileChecked()) return false;
    const auto before = quick;
    if (!quick.pin(target, pinned) || !save()) { quick = before; return false; }
    return true;
}
void sakuraForget(SakuraActionKind kind, uint32_t id) {
    if (!sakuraSetPinned(kind, id, false)) displayError("Favorites not saved", true);
}
void sakuraEditFavoritesMenu() {
    load();
    for (;;) {
        std::vector<Option> choices = {{"Pin section", addSections}, {"Pin safe screen", addScreens},
                                       {"Pin SSH profile", addSshProfiles}, {"Pin QR", addQrCodes}};
        for (unsigned i = 0; i < quick.count; ++i) {
            const auto target = quick.pins[i];
            String title = label(target);
            if (title.isEmpty()) title = "Unavailable (remove)";
            choices.push_back({title, [target]() { editPin(target); }});
        }
        choices.push_back({"Back", []() {}});
        const int selected = loopOptions(choices, MENU_TYPE_SUBMENU, "Edit Favorites");
        if (returnToMenu || selected < 0 || selected == int(choices.size()) - 1) return;
    }
}
void sakuraFavoritesMenu() {
    load();
    returnToMenu = false;
    for (;;) {
        std::vector<Option> choices;
        for (unsigned i = 0; i < quick.count; ++i) {
            const auto target = quick.pins[i];
            const String title = label(target);
            if (!title.isEmpty()) choices.push_back({title, [target]() { open(target); }});
        }
        choices.push_back({"Recent screens", sakuraRecentMenu});
        choices.push_back({"Edit Favorites", sakuraEditFavoritesMenu});
        choices.push_back({"Back", []() {}});
        const int selected = loopOptions(choices, MENU_TYPE_SUBMENU, "Favorites");
        if (returnToMenu || selected < 0 || selected == int(choices.size()) - 1) return;
    }
}
void sakuraRecentMenu() {
    load();
    for (;;) {
        std::vector<Option> choices;
        for (unsigned i = 0; i < quick.recentCount; ++i) {
            const auto target = quick.recent[i];
            const String title = label(target);
            if (!title.isEmpty()) choices.push_back({title, [target]() { open(target); }});
        }
        choices.push_back({String("History: ") + (quick.history ? "On" : "Off"), []() {
            const auto before = quick;
            quick.history = !quick.history;
            if (!quick.history) quick.recentCount = 0;
            if (!save()) { quick = before; displayError("History setting not saved", true); }
        }});
        choices.push_back({"Clear history", []() { quick.recentCount = 0; }});
        choices.push_back({"Back", []() {}});
        const int selected = loopOptions(choices, MENU_TYPE_SUBMENU, "Recent screens");
        if (returnToMenu || selected < 0 || selected == int(choices.size()) - 1) return;
    }
}
void sakuraRecordMenu(const char *menu, const char *selected) {
    if (!menu || !selected) return;
    load();
    if (strcmp(menu, "Main Menu") == 0) {
        for (unsigned id = 0; id < 15; ++id) if (strcmp(selected, sections[id]) == 0) quick.record({1, id});
    }
    if (strcmp(selected, "Buttons") == 0 && strcmp(menu, "Diagnostics") == 0) quick.record({2, 1});
    if (strcmp(selected, "Sakura Appearance") == 0 && strcmp(menu, "Config") == 0) quick.record({2, 2});
    if (strcmp(selected, "QRCodes") == 0 && strcmp(menu, "Others") == 0) quick.record({2, 3});
    if (strcmp(selected, "SSH Profiles") == 0 && strcmp(menu, "WiFi") == 0) quick.record({2, 4});
}
#endif
