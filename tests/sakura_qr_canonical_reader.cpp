#include <cassert>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cstring>
#include <cstdint>
#include <ArduinoJson.h>
struct String : std::string {
    using std::string::string;
    String() = default;
    String(const std::string &s) : std::string(s) {}
    bool reserve(size_t n) { std::string::reserve(n); return true; }
    size_t write(uint8_t c) { push_back(char(c)); return 1; }
    size_t write(const uint8_t *p, size_t n) { append((const char *)p,n); return n; }
};
void convertToJson(const String &s, JsonVariant v) { v.set(std::string(s)); }
void convertFromJson(JsonVariantConst v, String &s) { s=v.as<std::string>(); }
bool canConvertFromJson(JsonVariantConst v, const String &) { return v.is<const char*>(); }
#define FILE_READ 0
#define FILE_WRITE 1
#define log_e(...) ((void)0)
std::map<std::string,std::string> disk;
bool available=true, readWorks=true, writeWorks=true;
struct File {
    String path;
    bool ok=false;
    size_t pos=0;
    operator bool() const { return ok; }
    void close() {}
    void flush() {}
    size_t size() { return disk[path].size(); }
    size_t write(const uint8_t *p, size_t n) {
        if (!writeWorks && n) --n;
        disk[path].assign((const char*)p,n); return n;
    }
    size_t read(uint8_t *p, size_t n) {
        n=std::min(n,disk[path].size()-pos);
        memcpy(p,disk[path].data()+pos,n); pos+=n; return n;
    }
    int read() { return pos<disk[path].size()?uint8_t(disk[path][pos++]):-1; }
    size_t readBytes(char *p,size_t n) { return read((uint8_t*)p,n); }
};
struct FS {
    size_t totalBytes() { return available?40960:0; }
    bool exists(const char *p) { return disk.count(p); }
    File open(const String &p,int mode) {
        File f; f.path=p;
        if (mode==FILE_WRITE) { disk[p].clear(); f.ok=true; }
        else f.ok=readWorks&&disk.count(p);
        return f;
    }
    bool remove(const String &p) { return disk.erase(p); }
    bool rename(const String &a,const String &b) { disk[b]=disk[a];disk.erase(a);return true; }
} LittleFS;
struct FluxConfig {
    struct QrCodeEntry { String menuName,content; uint32_t id; };
    std::vector<QrCodeEntry> qrCodes;
    uint32_t qrNextId=5;
    String btcReceiveAddress;
    bool _qrStorageValid=true;
    void loadQrJson(JsonObject);
    bool loadQrFile();
    bool saveQrFileChecked();
    bool addQrCodeEntry(const String &,const String &);
};
/* PRODUCTION_CONFIG_QR */
void boot(FluxConfig &c,const char *legacy) {
    if (!c.loadQrFile()) {
        JsonDocument d; assert(!deserializeJson(d,legacy)); c.loadQrJson(d.as<JsonObject>());
    }
}
int main() {
    const char *originalLegacy=R"({"qrNextId":42,"btcReceiveAddress":"","qrCodes":[{"id":8,"menuName":"Original","content":"original"}]})";
    const char *staleSd=R"({"qrNextId":9,"btcReceiveAddress":"","qrCodes":[{"id":8,"menuName":"Other","content":"wrong SD payload"}]})";
    const char *staleLittleFs=R"({"qrNextId":1,"btcReceiveAddress":"","qrCodes":[]})";
    FluxConfig first; boot(first,originalLegacy); assert(disk.empty()); assert(first.saveQrFileChecked());
    const auto original=disk["/sakura-qr.json"];
    FluxConfig reboot; boot(reboot,staleSd);
    assert(reboot.qrCodes.size()==1&&reboot.qrCodes[0].id==8&&reboot.qrCodes[0].content=="original"&&reboot.qrNextId==42);
    FluxConfig withoutSd; boot(withoutSd,staleLittleFs);
    assert(withoutSd.qrCodes.size()==1&&withoutSd.qrCodes[0].id==8&&withoutSd.qrNextId==42);
    assert(reboot.addQrCodeEntry("next","different"));
    assert(reboot.qrCodes.back().id==42&&reboot.qrCodes[0].content=="original");
    const auto committed=disk["/sakura-qr.json"];
    writeWorks=false; assert(!reboot.addQrCodeEntry("unsaved","unsaved"));
    assert(reboot.qrNextId==43&&disk["/sakura-qr.json"]==committed); writeWorks=true;
    FluxConfig afterFailure; boot(afterFailure,staleSd);
    assert(afterFailure.qrNextId==43&&afterFailure.qrCodes[0].id==8&&afterFailure.qrCodes[0].content=="original");
    for (const char *bad : {"{broken",R"({"qrCodes":"wrong","qrNextId":1,"btcReceiveAddress":""})",R"({"qrCodes":[1],"qrNextId":1,"btcReceiveAddress":""})"}) {
        disk["/sakura-qr.json"]=bad; FluxConfig invalid; boot(invalid,staleSd);
        assert(!invalid._qrStorageValid&&invalid.qrCodes.empty()&&invalid.qrNextId==0);
        assert(!invalid.saveQrFileChecked()&&disk["/sakura-qr.json"]==bad);
    }
    disk["/sakura-qr.json"]=original; readWorks=false;
    FluxConfig unreadable; boot(unreadable,staleSd);
    assert(!unreadable._qrStorageValid&&unreadable.qrCodes.empty()); readWorks=true;
    available=false; FluxConfig unavailable; boot(unavailable,staleSd);
    assert(!unavailable._qrStorageValid&&unavailable.qrCodes.empty()&&unavailable.qrNextId==0);
}
