#include "ble_whitelist.h"

#include "core/display.h"
#include "core/mykeyboard.h"
#include "core/utils.h"
#include <Preferences.h>
#include <algorithm>
#include <cctype>
#include <globals.h>
#include <vector>

namespace {
constexpr const char *kNamespace = "ble_wl";
constexpr size_t kMaxEntries = 32;

String normalizeMac(String value) {
    value.trim();
    value.toUpperCase();
    value.replace('-', ':');
    return value;
}

bool validMac(const String &value) {
    if (value.length() != 17) return false;
    for (int i = 0; i < 17; ++i) {
        if (i % 3 == 2) {
            if (value[i] != ':') return false;
        } else if (!isxdigit(static_cast<unsigned char>(value[i]))) {
            return false;
        }
    }
    return true;
}

std::vector<String> loadEntries() {
    std::vector<String> entries;
    Preferences prefs;
    if (!prefs.begin(kNamespace, true)) return entries;
    uint8_t count = prefs.getUChar("count", 0);
    for (uint8_t i = 0; i < count && entries.size() < kMaxEntries; ++i) {
        String value = normalizeMac(prefs.getString((String("m") + i).c_str(), ""));
        if (validMac(value) && std::find(entries.begin(), entries.end(), value) == entries.end()) entries.push_back(value);
    }
    prefs.end();
    return entries;
}

void saveEntries(const std::vector<String> &entries) {
    Preferences prefs;
    if (!prefs.begin(kNamespace, false)) return;
    prefs.clear();
    prefs.putUChar("count", static_cast<uint8_t>(std::min(entries.size(), kMaxEntries)));
    for (size_t i = 0; i < entries.size() && i < kMaxEntries; ++i)
        prefs.putString((String("m") + i).c_str(), entries[i]);
    prefs.end();
}
}

bool bleWhitelistContains(const String &address) {
    String normalized = normalizeMac(address);
    if (!validMac(normalized)) return false;
    for (const String &entry : loadEntries())
        if (entry == normalized) return true;
    return false;
}

void bleWhitelistMenu() {
    while (true) {
        std::vector<String> entries = loadEntries();
        options.clear();
        options.push_back({"Add MAC", []() {
                               String value = normalizeMac(keyboard("", 17, "MAC AA:BB:CC:DD:EE:FF"));
                               if (!validMac(value)) {
                                   displayError("Invalid MAC address");
                                   delay(800);
                                   return;
                               }
                               std::vector<String> current = loadEntries();
                               if (std::find(current.begin(), current.end(), value) == current.end() && current.size() < kMaxEntries) {
                                   current.push_back(value);
                                   saveEntries(current);
                               }
                           }});
        for (const String &entry : entries) {
            options.push_back({entry.c_str(), [entry]() {
                                   std::vector<String> current = loadEntries();
                                   current.erase(std::remove(current.begin(), current.end(), entry), current.end());
                                   saveEntries(current);
                               }});
        }
        const int backIndex = static_cast<int>(options.size());
        options.push_back({"Back", []() {}});
        addOptionToMainMenu();
        const int selected = loopOptions(options, MENU_TYPE_SUBMENU, "BLE Whitelist", 0, false);
        if (selected < 0 || selected >= backIndex || returnToMenu) return;
        if (options.empty()) return;
    }
}
