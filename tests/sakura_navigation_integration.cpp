#include <cassert>
#include <vector>
#include <string>
#include <functional>
#include <algorithm>
#include <cstring>
#include <cstdint>
#include <delta/quick_access.h>
#include "core/sakura_navigation.h"
#define SAKURA_BUILD 1
#define MENU_TYPE_SUBMENU 1
struct String : std::string {
    using std::string::string;
    String() = default;
    String(const std::string &value) : std::string(value) {}
    bool isEmpty() const { return empty(); }
};
bool returnToMenu = false;
int interpreter_state = 0, writes = 0, loops = 0, resumes = 0;
struct Option {
    String label;
    std::function<void()> operation;
    bool selected = false;
    Option(String text, std::function<void()> fn, bool state = false) : label(text), operation(fn), selected(state) {}
};
struct Preferences {
    bool begin(const char *, bool read) { return !read; }
    size_t getBytesLength(const char *) { return 0; }
    size_t getBytes(const char *, uint8_t *, size_t) { return 0; }
    size_t putBytes(const char *, uint8_t *, size_t count) { ++writes; return count; }
    void end() {}
};
struct MenuItemInterface {
    String name = "WiFi";
    String getName() { return name; }
    void optionsMenu() {
        if (name == "JS Interpreter" && interpreter_state >= 0) { interpreter_state = 1; ++resumes; }
        returnToMenu = true;
    }
};
MenuItemInterface wifi, scripts;
struct MainMenu { std::vector<MenuItemInterface *> getItems() { return {&wifi, &scripts}; } } mainMenu;
struct Config {
    std::vector<String> disabledMenus;
    bool canonicalWriteOk = true;
    int canonicalWrites = 0;
    bool saveQrFileChecked() { ++canonicalWrites; return canonicalWriteOk; }
} fluxConfig;
uint32_t deltaUiFavoriteSections() { return 1; }
void inputButtonsScreen() {}
void deltaAppearanceMenu() {}
void qrcode_menu() {}
struct SshProfileLink { uint32_t id; String name; };
std::vector<SshProfileLink> profiles = {{73, "First name"}};
std::vector<SshProfileLink> sshProfileLinks() { return profiles; }
int profileDetails = 0, profileLists = 0;
bool openSshProfile(uint32_t id) { if (id == 73) ++profileDetails; return id == 73; }
void sshProfilesMenu() { ++profileLists; }
struct SavedQrLink { uint32_t id; String name; };
std::vector<SavedQrLink> codes = {{9, "My link"}};
std::vector<SavedQrLink> savedQrLinks() { return codes; }
int qrViews = 0;
bool openSavedQr(uint32_t id) { if (id == 9) ++qrViews; return id == 9; }
void displayError(const char *, bool) {}
int loopOptions(std::vector<Option> &choices, int, const char *, int = 0) {
    if (++loops == 1) { choices[0].operation(); return 0; }
    return -1;
}
/* PRODUCTION_NAVIGATION */
int main() {
    scripts.name = "JS Interpreter";
    assert(sakuraIsPinned(SakuraActionKind::Section, 0) && writes == 0);
    for (int i = 0; i < 100; ++i) sakuraRecordMenu("Main Menu", "WiFi");
    assert(writes == 0 && quick.recentCount == 1);
    assert(section(11) == nullptr);
    open({1, 11});
    assert(interpreter_state == 0 && resumes == 0);
    interpreter_state = -1;
    assert(section(11) == &scripts);
    fluxConfig.disabledMenus.push_back("WiFi");
    assert(section(0) == nullptr);
    fluxConfig.disabledMenus.clear();
    assert(section(3) == nullptr && section(99) == nullptr);
    sakuraFavoritesMenu();
    assert(loops == 1 && returnToMenu);
    assert(sakuraSetPinned(SakuraActionKind::Screen, 1, true) && writes == 1);
    assert(sakuraIsPinned(SakuraActionKind::Section, 0));
    assert(label({3,73}) == "SSH: First name" && label({3,99}).isEmpty());
    profiles[0].name = "Renamed";
    assert(label({3,73}) == "SSH: Renamed");
    const auto recentBefore = quick.recentCount;
    open({3,73});
    assert(profileDetails == 1 && quick.recentCount == recentBefore);
    open({2,4});
    assert(profileLists == 1 && quick.recent[0] == delta::QuickAction(2,4));
    assert(label({4,9}) == "QR: My link" && label({4,99}).isEmpty());
    codes[0].name = "Renamed QR";
    assert(label({4,9}) == "QR: Renamed QR");
    const auto beforeQr = quick.recentCount;
    open({4,9});
    assert(qrViews == 1 && quick.recentCount == beforeQr);
    fluxConfig.canonicalWriteOk = false;
    assert(!sakuraSetPinned(SakuraActionKind::Qr,9,true));
    assert(!sakuraIsPinned(SakuraActionKind::Qr,9) && writes == 1);
    fluxConfig.canonicalWriteOk = true;
    assert(sakuraSetPinned(SakuraActionKind::Qr,9,true) && writes == 2);
    assert(fluxConfig.canonicalWrites == 2);
}
