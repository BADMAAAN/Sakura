"""Execute saved-QR mutations and editor cancellation using production bodies."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


class MyQrTests(unittest.TestCase):
    def test_canonical_reader_reboot_and_sd_removal(self):
        config = (ROOT / 'src/core/config.cpp').read_text(encoding='utf-8')
        bodies = '\n'.join(function(config, signature) for signature in (
            'void FluxConfig::loadQrJson(', 'bool FluxConfig::loadQrFile(',
            'bool FluxConfig::saveQrFileChecked(', 'bool FluxConfig::addQrCodeEntry('))
        fixture = (ROOT / 'tests/sakura_qr_canonical_reader.cpp').read_text(encoding='utf-8')
        source = fixture.replace('/* PRODUCTION_CONFIG_QR */', bodies)
        zig = next((ROOT / '.tools/zig').rglob('zig.exe'))
        with tempfile.TemporaryDirectory(prefix='sakura-canonical-') as directory:
            exe = Path(directory) / 'canonical.exe'
            subprocess.run([str(zig), 'c++', '-std=c++11', '-O1', '-UNDEBUG', '-I',
                            str(ROOT / '.pio/libdeps/flux-delta/ArduinoJson/src'),
                            '-x', 'c++', '-', '-o', str(exe)], input=source, text=True, check=True)
            subprocess.run([str(exe)], check=True)

    def test_saved_identity_edit_cancel_delete_and_wallet(self):
        config = (ROOT / 'src/core/config.cpp').read_text(encoding='utf-8')
        header = (ROOT / 'src/core/config.h').read_text(encoding='utf-8')
        menu = (ROOT / 'src/modules/others/qrcode_menu.cpp').read_text(encoding='utf-8')
        self.assertIn('String btcReceiveAddress = "";', header)
        self.assertIn('qrEntry["id"] = entry.id;', config)
        self.assertIn('setting["qrNextId"] = qrNextId;', config)
        self.assertIn('qrNextId = setting["qrNextId"] | uint32_t(1);', config)
        source = r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>
struct String : std::string {
 using std::string::string;
 String(const std::string &s) : std::string(s) {}
 bool reserve(size_t n) { std::string::reserve(n); return true; }
 bool isEmpty() const { return empty(); }
};
int storageFailure = 0, commits = 0;
std::map<std::string, std::string> disk;
#define FILE_WRITE 1
#define FILE_READ 0
struct File {
 String path; bool valid = false; size_t position = 0;
 File() {} File(const String &p, bool v) : path(p), valid(v) {}
 operator bool() const { return valid; }
 size_t size() const { return valid ? disk[path].size() : 0; }
 size_t write(const uint8_t *data, size_t n) {
  if (storageFailure == 2 && n) --n;
  disk[path].assign(reinterpret_cast<const char *>(data), n); return n;
 }
 void flush() { if (storageFailure == 3 && !disk[path].empty()) disk[path][0] ^= 1; }
 void close() {}
 size_t read(uint8_t *out, size_t n) {
  n = std::min(n, disk[path].size() - position);
  std::memcpy(out, disk[path].data() + position, n); position += n;
  if (storageFailure == 5 && n) out[0] ^= 1;
  return n;
 }
};
struct FS {
 File open(const String &path, int mode) {
  if ((mode == FILE_WRITE && storageFailure == 1) ||
      (mode == FILE_READ && storageFailure == 4)) return {};
  if (mode == FILE_WRITE) disk[path].clear();
  return File(path, disk.count(path));
 }
 bool rename(const String &from, const String &to) {
  if (storageFailure == 6) return false;
  disk[to] = disk[from]; disk.erase(from); ++commits; return true;
 }
 bool remove(const String &path) { return disk.erase(path); }
 unsigned totalBytes() { return storageFailure == 7 ? 0 : 40960; }
} LittleFS;
bool getFsStorage(FS *&fs) { fs = &LittleFS; return storageFailure != 7; }
using JsonFields = std::map<std::string, String>;
using JsonRows = std::vector<JsonFields>;
struct JsonValue {
 String *value; JsonRows *rows;
 JsonValue &operator=(uint32_t n) { *value = std::to_string(n); return *this; }
 JsonValue &operator=(const String &s) { *value = s; return *this; }
 template<class T> T to() { return T{rows}; }
};
struct JsonObject {
 JsonFields *fields;
 JsonValue operator[](const char *name) { return {&(*fields)[name], nullptr}; }
};
struct JsonArray {
 JsonRows *rows;
 template<class T> T add() { rows->push_back({}); return T{&rows->back()}; }
};
struct JsonDocument {
 String payload; JsonFields fields; JsonRows rows;
 JsonValue operator[](const char *name) { return {&fields[name], &rows}; }
 bool overflowed() const { return storageFailure == 8; }
};
String jsonPayload(const JsonDocument &doc) {
 if (!doc.payload.empty()) return doc.payload;
 String result = doc.fields.at("qrNextId") + ":" + doc.fields.at("btcReceiveAddress");
 for (const auto &row : doc.rows)
  result += "|" + row.at("id") + ":" + row.at("menuName") + ":" + row.at("content");
 return result;
}
size_t measureJson(const JsonDocument &doc) { return jsonPayload(doc).size(); }
size_t serializeJson(const JsonDocument &doc, String &out) {
 out = jsonPayload(doc);
 if (storageFailure == 9 && !out.empty()) out.pop_back();
 return out.size();
}
struct FluxConfig {
 struct QrCodeEntry { String menuName, content; uint32_t id; };
 std::vector<QrCodeEntry> qrCodes;
 uint32_t qrNextId = 1;
 String btcReceiveAddress;
 const char *filepath = "/flux.conf";
 int saves = 0;
 bool _qrStorageValid = true;
 void saveFile() { ++saves; }
 JsonDocument toJson() const {
  JsonDocument doc; doc.payload = std::to_string(qrNextId) + ":" + btcReceiveAddress;
  for (const auto &entry : qrCodes)
   doc.payload += "|" + std::to_string(entry.id) + ":" + entry.menuName + ":" + entry.content;
  return doc;
 }
 bool saveQrFileChecked();
 bool addQrCodeEntry(const String &, const String &);
 bool updateQrCodeEntry(uint32_t, const String &, const String &);
 bool removeQrCodeEntry(uint32_t);
 bool removeQrCodeEntry(const String &);
 bool setBtcReceiveAddress(const String &);
} fluxConfig;
struct QRcode { static constexpr unsigned maxPayloadBytes = 154; };
struct SavedQrLink { uint32_t id; String name; };
int errors = 0, rendered = 0;
#define SAKURA_BUILD
enum class SakuraActionKind { Qr };
int forgotten = 0;
void sakuraForget(SakuraActionKind, uint32_t) { ++forgotten; }
void displayError(const char *, bool = false) { ++errors; }
void delay(int) {}
void qrcode_display(const String &) { ++rendered; }
std::vector<String> keys;
String keyboard(const String &, unsigned, const char *) {
 assert(!keys.empty()); String next = keys.front(); keys.erase(keys.begin()); return next;
}
bool bitcoinSha256(const uint8_t *, size_t, uint8_t *) { return false; }
namespace sakura {
 template<class T> bool validBitcoinAddress(const char *s, T) {
  return std::strcmp(s, "1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa") == 0;
 }
}
'''
        for signature in ('bool FluxConfig::saveQrFileChecked(', 'bool FluxConfig::addQrCodeEntry(',
                          'bool FluxConfig::updateQrCodeEntry(',
                          'bool FluxConfig::removeQrCodeEntry(uint32_t',
                          'bool FluxConfig::removeQrCodeEntry(const String',
                          'bool FluxConfig::setBtcReceiveAddress('):
            source += '\n' + function(config, signature)
        for signature in ('bool validQrText(', 'void editSavedQr(', 'void deleteSavedQr(', 'void setBitcoinAddress(',
                          'std::vector<SavedQrLink> savedQrLinks(', 'bool openSavedQr(',
                          'void display_custom_qrcode(', 'void save_and_display_qrcode('):
            source += '\n' + function(menu, signature)
        # Execute the exact legacy migration loop used after decoding config.
        migration = function(config, 'for (auto &entry : qrCodes)')
        source += '\nvoid migrate() { using QrCodeEntry = FluxConfig::QrCodeEntry; auto &qrCodes = fluxConfig.qrCodes; auto &qrNextId = fluxConfig.qrNextId;'
        source += migration + '}\n'
        decoded_start = config.index('if (id && std::any_of(qrCodes.begin()')
        decoded_end = config.index(';', config.index('if (qrNextId && id >= qrNextId)', decoded_start)) + 1
        source += '\nvoid decodeEntry(const String &menuName, const String &content, uint32_t id) {'
        source += 'using QrCodeEntry = FluxConfig::QrCodeEntry; auto &qrCodes = fluxConfig.qrCodes; auto &qrNextId = fluxConfig.qrNextId;'
        source += config[decoded_start:decoded_end] + '}\n'
        source += r'''
int main() {
 // A true legacy file has no high-water key, so its initial value is 1.
 fluxConfig.qrCodes.clear(); fluxConfig.qrNextId = 1;
 decodeEntry("Legacy A", "a", 0); decodeEntry("Legacy B", "b", 0); migrate();
 assert(fluxConfig.qrCodes[0].id == 1 && fluxConfig.qrCodes[1].id == 2);
 assert(fluxConfig.qrNextId == 3 && fluxConfig.saves == 0);
 // IDs below the persisted high-water mark may belong to stale Favorites links.
 fluxConfig.qrCodes.clear(); fluxConfig.qrNextId = 100;
 decodeEntry("Valid", "v", 8); decodeEntry("Invalid", "i", 0);
 decodeEntry("Missing", "m", 0); decodeEntry("Duplicate", "d", 8); migrate();
 assert(fluxConfig.qrCodes[0].id == 8 && fluxConfig.qrCodes[1].id == 100);
 assert(fluxConfig.qrCodes[2].id == 101 && fluxConfig.qrCodes[3].id == 102);
 assert(fluxConfig.qrNextId == 103 && fluxConfig.saves == 0);
 // At exhaustion keep every payload and valid ID; never wrap to retired ID 1.
 fluxConfig.qrCodes = {{"Valid", "v", 8}, {"Last", "l", 0}, {"Pending", "p", 0}};
 fluxConfig.qrNextId = UINT32_MAX; migrate();
 assert(fluxConfig.qrCodes[1].id == UINT32_MAX && fluxConfig.qrCodes[2].id == 0);
 assert(fluxConfig.qrNextId == 0 && fluxConfig.qrCodes[2].content == "p");
 migrate(); fluxConfig.addQrCodeEntry("overflow", "overflow");
 assert(fluxConfig.qrCodes.size() == 3 && fluxConfig.saves == 0);
 fluxConfig.qrCodes.clear(); fluxConfig.qrNextId = 1;
 decodeEntry("Valid", "v", UINT32_MAX); decodeEntry("Pending", "p", 0); migrate();
 assert(fluxConfig.qrCodes[0].id == UINT32_MAX && fluxConfig.qrCodes[1].id == 0);
 assert(fluxConfig.qrNextId == 0 && fluxConfig.saves == 0);
 fluxConfig.qrCodes = {{"Legacy A", "a", 0}, {"Legacy B", "b", 0}, {"Saved", "c", 8}};
 fluxConfig.qrNextId = 9; migrate();
 assert(fluxConfig.qrCodes[0].id == 9 && fluxConfig.qrCodes[1].id == 10);
 assert(fluxConfig.qrCodes[2].id == 8 && fluxConfig.saves == 0);
 migrate(); assert(fluxConfig.qrCodes[0].id == 9 && fluxConfig.saves == 0);
 fluxConfig.addQrCodeEntry("new", "https://example.invalid");
 assert(fluxConfig.qrCodes.back().id == 11 && fluxConfig.qrNextId == 12);
 assert(fluxConfig.updateQrCodeEntry(11, "renamed", "UTF8"));
 assert(fluxConfig.qrCodes.back().id == 11 && fluxConfig.qrNextId == 12);
 assert(!fluxConfig.updateQrCodeEntry(99, "lost", "lost"));
 int saves = fluxConfig.saves;
 keys = {"renamed again", "\x1b"}; editSavedQr(11);
 assert(fluxConfig.qrCodes.back().menuName == "renamed" && fluxConfig.saves == saves);
 keys = {"\x1b"}; editSavedQr(11); assert(fluxConfig.saves == saves);
 keys = {"edited", "new text"}; editSavedQr(11);
 assert(fluxConfig.qrCodes.back().id == 11 && fluxConfig.qrCodes.back().content == "new text");
 auto links = savedQrLinks(); assert(links.back().id == 11 && links.back().name == "edited");
 assert(openSavedQr(11) && rendered == 1);
 assert(!openSavedQr(99) && rendered == 1);
 keys = {"\x1b"}; display_custom_qrcode(); assert(rendered == 1);
 keys = {"new draft", "\x1b"}; save_and_display_qrcode();
 assert(fluxConfig.qrCodes.size() == 4 && rendered == 1);
 keys = {"new draft", ""}; save_and_display_qrcode();
 assert(fluxConfig.qrCodes.size() == 4 && rendered == 1);
 String limit; for (int i=0;i<77;++i) limit += "\xd1\x8f";
 assert(limit.size() == 154 && validQrText(limit));
 limit += "x"; assert(!validQrText(limit));
 assert(!validQrText(String("a\0b", 3)));
 fluxConfig.removeQrCodeEntry(uint32_t(11)); assert(!openSavedQr(11));
 fluxConfig.addQrCodeEntry("next", "next"); assert(fluxConfig.qrCodes.back().id == 12);
 fluxConfig.qrCodes.clear(); assert(savedQrLinks().empty() && !openSavedQr(12));
 assert(fluxConfig.btcReceiveAddress.empty());
 saves = fluxConfig.saves;
 keys = {"bc1qinvalid"}; setBitcoinAddress();
 assert(fluxConfig.btcReceiveAddress.empty() && fluxConfig.saves == saves);
 keys = {"\x1b"}; setBitcoinAddress(); assert(fluxConfig.saves == saves);
 keys = {"1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa"}; setBitcoinAddress();
 assert(!fluxConfig.btcReceiveAddress.empty());
 keys = {"bad"}; setBitcoinAddress();
 assert(fluxConfig.btcReceiveAddress == "1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa");
 // Every persistence failure leaves the original file, RAM, IDs and Favorites intact.
 for (int failure = 1; failure <= 9; ++failure) {
  storageFailure = 0;
  fluxConfig.qrCodes = {{"Pinned", "original", 5}}; fluxConfig.qrNextId = 6;
  fluxConfig.btcReceiveAddress = "";
  disk["/sakura-qr.json"] = fluxConfig.toJson().payload;
  const std::string original = disk["/sakura-qr.json"];
  const int oldCommits = commits, oldForgotten = forgotten, oldRendered = rendered;
  storageFailure = failure;
  assert(!fluxConfig.addQrCodeEntry("draft", "draft"));
  assert(fluxConfig.qrCodes.size() == 1 && fluxConfig.qrNextId == 6);
  assert(!fluxConfig.updateQrCodeEntry(5, "changed", "changed"));
  assert(fluxConfig.qrCodes[0].menuName == "Pinned" && fluxConfig.qrCodes[0].content == "original");
  deleteSavedQr(5);
  assert(forgotten == oldForgotten && fluxConfig.qrCodes.size() == 1);
  assert(!fluxConfig.removeQrCodeEntry(String("Pinned")) && fluxConfig.qrCodes.size() == 1);
  assert(!fluxConfig.setBtcReceiveAddress("new address") && fluxConfig.btcReceiveAddress.empty());
  keys = {"unsaved", "unsaved"}; save_and_display_qrcode();
  assert(rendered == oldRendered && fluxConfig.qrNextId == 6);
  assert(disk["/sakura-qr.json"] == original && commits == oldCommits);
  assert(disk.count("/sakura-qr.json.tmp") == 0);
  storageFailure = 0;
  assert(fluxConfig.addQrCodeEntry("committed", "committed"));
  assert(fluxConfig.qrCodes.back().id == 6 && fluxConfig.qrNextId == 7);
  assert(disk["/sakura-qr.json"] == fluxConfig.toJson().payload);
  deleteSavedQr(5); assert(forgotten == oldForgotten + 1);
  assert(fluxConfig.qrCodes.size() == 1 && fluxConfig.qrCodes[0].id == 6);
 }
 storageFailure = 7;
 assert(savedQrLinks().empty() && !openSavedQr(6)); // Boot may return before fromFile on mount failure.
 storageFailure = 0;
 fluxConfig._qrStorageValid = false;
 const std::string corrupt = "invalid canonical data";
 disk["/sakura-qr.json"] = corrupt;
 assert(!fluxConfig.saveQrFileChecked());
 assert(!fluxConfig.addQrCodeEntry("blocked", "blocked"));
 assert(disk["/sakura-qr.json"] == corrupt);
}
'''
        workspace = next(path for path in (ROOT, *ROOT.parents) if (path / '.tools/zig').is_dir())
        zig = next((workspace / '.tools/zig').rglob('zig.exe'))
        with tempfile.TemporaryDirectory() as directory:
            cpp, exe = Path(directory) / 'qr.cpp', Path(directory) / 'qr.exe'
            cpp.write_text(source, encoding='utf-8')
            subprocess.run([str(zig), 'c++', '-std=c++11', '-O2', '-UNDEBUG',
                            str(cpp), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)


if __name__ == '__main__':
    unittest.main()
