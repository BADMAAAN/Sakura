#include "ssh_profiles.h"
#include "clients.h"
#include "core/display.h"
#include "core/mykeyboard.h"
#include "core/sakura_navigation.h"
#include <Preferences.h>
#include <cstring>
#include <globals.h>

bool sshValidPort(const char *text, uint16_t &port) {
    if (!text || !*text) return false;
    unsigned value = 0;
    for (const char *p = text; *p; ++p) {
        if (*p < '0' || *p > '9' || value > 6553) return false;
        value = value * 10 + (*p - '0');
        if (value > 65535) return false;
    }
    if (!value) return false;
    port = static_cast<uint16_t>(value);
    return true;
}

bool sshValidEndpoint(const char *host, const char *user) {
    if (!host || !*host || !user || !*user || strlen(host) > 100 || strlen(user) > 64) return false;
    for (const char *p = host; *p; ++p)
        if (!( (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
               (*p >= '0' && *p <= '9') || *p == '.' || *p == '-' || *p == ':' )) return false;
    for (const char *p = user; *p; ++p) if (static_cast<unsigned char>(*p) <= 32 || *p == 127) return false;
    return true;
}

#ifndef LITE_VERSION
namespace {
constexpr size_t MAX_PROFILES = 12;
// One bounded NVS blob; passwords cannot enter this storage type.
struct Profile { uint32_t id; uint16_t port; char name[33]; char host[101]; char user[65]; };
struct Store { uint32_t magic = 0x53534831; uint32_t nextId = 1; uint8_t count = 0; Profile entries[MAX_PROFILES] = {}; };

bool validProfile(const Profile &p) {
    return p.id && p.port && p.name[0] && memchr(p.name, 0, sizeof(p.name)) &&
           memchr(p.host, 0, sizeof(p.host)) && memchr(p.user, 0, sizeof(p.user)) &&
           sshValidEndpoint(p.host, p.user);
}
Store load() {
    Store store;
    Preferences prefs;
    if (!prefs.begin("sakura_ssh", true)) return store;
    if (prefs.getBytesLength("profiles") == sizeof(Store)) {
        Store candidate;
        if (prefs.getBytes("profiles", &candidate, sizeof(candidate)) == sizeof(candidate) &&
            candidate.magic == store.magic && candidate.count <= MAX_PROFILES && candidate.nextId) {
            bool valid = true;
            for (size_t i = 0; i < candidate.count; ++i) {
                valid = valid && validProfile(candidate.entries[i]) && candidate.entries[i].id < candidate.nextId;
                for (size_t j = 0; j < i; ++j) valid = valid && candidate.entries[i].id != candidate.entries[j].id;
            }
            if (valid) store = candidate;
        }
    }
    prefs.end();
    return store;
}
bool save(const Store &store) {
    Preferences prefs;
    if (!prefs.begin("sakura_ssh", false)) { displayError("Profile storage unavailable", true); return false; }
    bool ok = prefs.putBytes("profiles", &store, sizeof(store)) == sizeof(store);
    prefs.end();
    if (!ok) displayError("Profile save failed", true);
    return ok;
}
bool edit(Profile &profile) {
    String name = keyboard(profile.name, 32, "Profile name");
    if (name == "\x1b") return false;
    String host = keyboard(profile.host, 100, "SSH host");
    if (host == "\x1b") return false;
    String portText = num_keyboard(String(profile.port ? profile.port : 22), 5, "SSH port");
    if (portText == "\x1b") return false;
    String user = keyboard(profile.user, 64, "SSH user");
    if (user == "\x1b") return false;
    name.trim(); host.trim(); user.trim();
    uint16_t port;
    if (name.isEmpty() || !sshValidEndpoint(host.c_str(), user.c_str()) || !sshValidPort(portText.c_str(), port)) {
        displayError("Name, host, user required; port 1-65535", true); return false;
    }
    name.toCharArray(profile.name, sizeof(profile.name));
    host.toCharArray(profile.host, sizeof(profile.host));
    user.toCharArray(profile.user, sizeof(profile.user));
    profile.port = port;
    return true;
}
bool confirmDelete() {
    bool confirmed = false;
    std::vector<Option> menu = {{"Cancel", []() {}}, {"Delete profile", [&]() { confirmed = true; }}};
    loopOptions(menu, MENU_TYPE_SUBMENU, "Delete SSH profile?");
    return confirmed;
}
}

std::vector<SshProfileLink> sshProfileLinks() {
    std::vector<SshProfileLink> links;
    Store store = load();
    for (size_t i = 0; i < store.count; ++i) links.push_back({store.entries[i].id, store.entries[i].name});
    return links;
}

bool openSshProfile(uint32_t id) {
    while (true) {
        Store store = load();
        size_t index = 0;
        while (index < store.count && store.entries[index].id != id) ++index;
        if (index == store.count) return false;
        Profile profile = store.entries[index];
        bool back = false;
        std::vector<Option> menu;
        menu.push_back({(String("Host: ") + profile.host).c_str(), []() {}});
        menu.push_back({(String("Port: ") + profile.port).c_str(), []() {}});
        menu.push_back({(String("User: ") + profile.user).c_str(), []() {}});
        menu.push_back({"Connect", [profile]() { ssh_connect_profile(profile.host, profile.port, profile.user); }});
        menu.push_back({"Edit", [&]() { if (edit(profile)) { store.entries[index] = profile; save(store); } }});
#ifdef SAKURA_BUILD
        bool pinned = sakuraIsPinned(SakuraActionKind::Ssh, id);
        menu.push_back({pinned ? "Unpin favorite" : "Pin favorite", [=]() {
            if (!sakuraSetPinned(SakuraActionKind::Ssh, id, !pinned)) displayError("Favorites not saved", true);
        }});
#endif
        menu.push_back({"Delete", [&]() {
            if (!confirmDelete()) return;
            for (size_t i = index + 1; i < store.count; ++i) store.entries[i - 1] = store.entries[i];
            store.entries[--store.count] = {};
            if (save(store)) {
#ifdef SAKURA_BUILD
                sakuraForget(SakuraActionKind::Ssh, id);
#endif
                back = true;
            }
        }});
        menu.push_back({"Back", [&]() { back = true; }});
        int selected = loopOptions(menu, MENU_TYPE_SUBMENU, profile.name);
        if (back || selected < 0 || returnToMenu) { returnToMenu = false; return true; }
    }
}

void sshProfilesMenu() {
    returnToMenu = false;
    while (true) {
        bool back = false;
        std::vector<Option> menu;
        menu.push_back({"Create profile", []() {
            Store store = load();
            if (store.count >= MAX_PROFILES || store.nextId == UINT32_MAX) { displayError("Profile limit reached", true); return; }
            Profile profile = {};
            if (edit(profile)) {
                profile.id = store.nextId++;
                store.entries[store.count++] = profile;
                save(store);
            }
        }});
        for (const auto &profile : sshProfileLinks()) {
            const uint32_t id = profile.id;
            menu.push_back({profile.name, [id]() { openSshProfile(id); }});
        }
        menu.push_back({"Back", [&]() { back = true; }});
        if (loopOptions(menu, MENU_TYPE_SUBMENU, "SSH Profiles") < 0 || back || returnToMenu) break;
    }
    returnToMenu = false;
}
#else
std::vector<SshProfileLink> sshProfileLinks() { return {}; }
bool openSshProfile(uint32_t) { return false; }
void sshProfilesMenu() {}
#endif
