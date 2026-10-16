#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <cstdlib>

struct String : std::string {
    using std::string::string;
    String() = default;
    String(const std::string &s) : std::string(s) {}
    void setCharAt(size_t i, char c) { at(i) = c; }
    bool isEmpty() const { return empty(); }
    void remove(size_t pos, size_t count) { erase(pos, count); }
};
static uint32_t clockMs;
static bool stopped, failed, autoTrust;
static String status, pendingHostFingerprint;
static int hostTrustDecision;
uint32_t millis() { return clockMs; }
void vTaskDelay(uint32_t ms) { clockMs += ms; if (autoTrust) hostTrustDecision = 1; }
#define pdMS_TO_TICKS(x) (x)
bool isStopRequested() { return stopped; }
void markSessionClosed(const String &text, bool error) { status = text; failed = error; }
void displayError(const char *, bool) {}
template <typename Fn> void withClientLock(Fn &&fn) { fn(); }
static std::vector<unsigned char> persisted;
static bool writeOk = true;
struct Preferences {
    bool begin(const char *, bool) { return true; }
    void end() {}
    size_t getBytesLength(const char *) { return persisted.size(); }
    size_t getBytes(const char *, void *value, size_t size) { memcpy(value, persisted.data(), size); return size; }
    size_t putBytes(const char *, const void *value, size_t size) {
        if (!writeOk) return 0;
        const auto *p = static_cast<const unsigned char *>(value);
        persisted.assign(p, p + size); return size;
    }
};
enum { SSH_OK = 0, SSH_ERROR = -1, SSH_AGAIN = -2, SSH_AUTH_SUCCESS = 10, SSH_AUTH_AGAIN = 11,
       SSH_KNOWN_HOSTS_OK = 20, SSH_KNOWN_HOSTS_UNKNOWN, SSH_KNOWN_HOSTS_NOT_FOUND,
       SSH_KNOWN_HOSTS_CHANGED, SSH_KNOWN_HOSTS_OTHER, SSH_PUBLICKEY_HASH_SHA256 = 30 };
using ssh_session = void *;
using ssh_channel = void *;
using ssh_key = void *;
static std::vector<int> writeResults;
static String delivered;
static int closeCalls, disconnectCalls, freeCalls, attachedFd = -1;
static bool returnToMenu;
static uint32_t workerDoneAt;
void requestSessionStop() { stopped = true; }
void *getClientTaskHandle() { return clockMs < workerDoneAt ? (void *)1 : nullptr; }
String getSessionStatus() { return status; }
bool didSessionFail() { return failed; }
void appendSessionLog(const String &) {}
void closeSessionLogUnlocked() {}
void displayWarning(const String &, bool wait) { assert(!wait || !getClientTaskHandle()); }
void displayError(const String &, bool) { assert(!getClientTaskHandle()); }
int ssh_channel_write(ssh_channel, const char *text, size_t length) {
    assert(!writeResults.empty()); int result = writeResults.front(); writeResults.erase(writeResults.begin());
    if (result > 0 && static_cast<size_t>(result) <= length) delivered.append(text, result);
    return result;
}
void ssh_disconnect(ssh_session) { ++disconnectCalls; attachedFd = -1; }
void ssh_free(ssh_session) { ++freeCalls; assert(attachedFd == -1); }
int close(int fd) { assert(fd == 42); ++closeCalls; return 0; }
static int known = SSH_KNOWN_HOSTS_OK, keyReads, storedKeys;
static bool keyOk = true, keySaveOk = true;
int ssh_session_is_known_server(ssh_session) { return known; }
int ssh_get_server_publickey(ssh_session, ssh_key *key) { ++keyReads; *key = (void *)1; return keyOk ? SSH_OK : SSH_ERROR; }
int ssh_get_publickey_hash(ssh_key, int, unsigned char **hash, size_t *length) { *hash = (unsigned char *)malloc(1); *length = 1; return SSH_OK; }
char *ssh_get_fingerprint_hash(int, unsigned char *, size_t) { char *p = (char *)malloc(15); strcpy(p, "SHA256:testkey"); return p; }
void ssh_key_free(ssh_key) {}
void ssh_clean_pubkey_hash(unsigned char **hash) { free(*hash); }
void ssh_string_free_char(char *text) { free(text); }
int ssh_session_update_known_hosts(ssh_session) { ++storedKeys; return keySaveOk ? SSH_OK : SSH_ERROR; }
/* PRODUCTION */

int main() {
    uint16_t port = 99;
    for (const char *bad : {"", "0", "65536", "-1", "22x", "999999999999999999", " 22"}) assert(!sshValidPort(bad, port));
    assert(sshValidPort("22", port) && port == 22);
    assert(sshValidPort("65535", port) && port == 65535);
    assert(!sshValidEndpoint("", "user") && !sshValidEndpoint("host", ""));
    assert(!sshValidEndpoint("bad host", "user") && !sshValidEndpoint("host", "bad\nuser"));
    assert(sshValidEndpoint("home.example", "user"));
    Store s = load(); assert(s.count == 0 && s.nextId == 1);
    Profile p = {}; p.id = s.nextId++; p.port = 22;
    strcpy(p.name, "iSH"); strcpy(p.host, "home.example"); strcpy(p.user, "user");
    assert(validProfile(p)); s.entries[s.count++] = p; assert(save(s));
    Store loaded = load(); assert(loaded.count == 1 && loaded.entries[0].id == 1 && loaded.nextId == 2);
    strcpy(loaded.entries[0].name, "Renamed"); assert(save(loaded));
    assert(load().entries[0].id == 1); // Editing preserves identity.
    loaded.count = 0; loaded.entries[0] = {}; assert(save(loaded));
    assert(load().nextId == 2 && load().count == 0); // Deleted ID cannot be reused.
    writeOk = false; loaded.count = 1; assert(!save(loaded)); assert(load().count == 0); writeOk = true;
    String password = "private-value"; clearSshPassword(password); assert(password.empty());
    int attempts = 0;
    assert(sshStep([&]() { return ++attempts == 3 ? SSH_OK : SSH_AGAIN; }, SSH_OK, SSH_AGAIN, "connect"));
    attempts = 0; clockMs = 0;
    assert(!sshStep([&]() { ++attempts; return SSH_AGAIN; }, SSH_OK, SSH_AGAIN, "connect", 50));
    assert(failed && clockMs >= 50 && clockMs < 100 && attempts == 3);
    stopped = true; attempts = 0;
    assert(!sshStep([&]() { ++attempts; return SSH_OK; }, SSH_OK, SSH_AGAIN, "connect"));
    assert(!attempts && !failed); stopped = false;
    assert(!sshStep([]() { return SSH_ERROR; }, SSH_AUTH_SUCCESS, SSH_AUTH_AGAIN, "authentication"));
    assert(failed && status.find("authentication") != std::string::npos);
    assert(verifySshServer(nullptr));
    known = SSH_KNOWN_HOSTS_CHANGED;
    assert(!verifySshServer(nullptr) && keyReads == 0 && storedKeys == 0);
    known = SSH_KNOWN_HOSTS_OTHER; assert(!verifySshServer(nullptr));
    known = SSH_KNOWN_HOSTS_UNKNOWN; autoTrust = false; clockMs = 0;
    assert(!verifySshServer(nullptr) && clockMs == 60000 && storedKeys == 0);
    autoTrust = true; assert(verifySshServer(nullptr) && storedKeys == 1);
    keySaveOk = false; assert(!verifySshServer(nullptr)); keySaveOk = true;
    keyOk = false; assert(!verifySshServer(nullptr)); keyOk = true;
    autoTrust = false; stopped = true; assert(!verifySshServer(nullptr));
    assert(pendingHostFingerprint.empty());
    stopped = false; clockMs = 0;
    String pending = "abcdef"; writeResults = {SSH_AGAIN, 0, 2, 1, 3};
    assert(sshFlushOutbound(nullptr, pending, 0) && pending == "abcdef");
    assert(sshFlushOutbound(nullptr, pending, 0) && pending == "abcdef");
    assert(sshFlushOutbound(nullptr, pending, 0) && pending == "cdef");
    assert(sshFlushOutbound(nullptr, pending, 0) && pending == "def");
    assert(sshFlushOutbound(nullptr, pending, 0) && pending.empty() && delivered == "abcdef");
    pending = "x"; clockMs = 15000;
    assert(!sshFlushOutbound(nullptr, pending, 0) && pending == "x");
    clockMs = 0; stopped = true;
    assert(!sshFlushOutbound(nullptr, pending, 0) && pending == "x");
    stopped = false; writeResults = {SSH_ERROR}; assert(!sshFlushOutbound(nullptr, pending, 0));
    const bool attachedCases[] = {true, false, false, true, true};
    // Normal session, cancel before connect, pre-attach failure, partial handshake, auth failure.
    for (bool attached : attachedCases) {
        ssh_session session = (void *)1; int fd = 42; attachedFd = attached ? fd : -1;
        int before = closeCalls;
        closeSshTransport(session, fd);
        assert(!session && fd == -1 && closeCalls == before + 1);
        closeSshTransport(session, fd); assert(closeCalls == before + 1);
    }
    assert(disconnectCalls == 5 && freeCalls == 5);
    ssh_session absent = nullptr; int fd = 42;
    closeSshTransport(absent, fd); assert(fd == -1 && closeCalls == 6);
    clockMs = 0; workerDoneAt = 4020; returnToMenu = false;
    finishSessionUi("SSH");
    assert(clockMs == workerDoneAt && returnToMenu && !getClientTaskHandle());
}
