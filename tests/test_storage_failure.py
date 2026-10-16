"""Execute the firmware's storage entry points with a failed LittleFS mount."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


def function(path, signature):
    text = (ROOT / path).read_text(encoding="utf-8")
    start = text.index(signature)
    opening = text.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


class StorageFailureTests(unittest.TestCase):
    def test_failed_mount_preserves_data_and_boots_with_defaults(self):
        compiler = os.environ.get("CXX") or shutil.which("g++") or shutil.which("clang++")
        zig = next((ROOT / ".tools/zig").rglob("zig.exe"), None)
        if not compiler and zig:
            compiler = str(zig)
        self.assertTrue(compiler, "Set CXX to a host C++ compiler (or place Zig under .tools/zig)")
        command = [compiler]
        if Path(compiler).stem == "zig":
            command += ["c++", "-nostdlib++"]
        source = r'''
#include <assert.h>
#include <stdint.h>
#define RAM_LOG(...) ((void)0)
#define log_e(...) ((void)0)
#define log_i(...) ((void)0)
#define FILE_WRITE 1
#define FILE_READ 0
struct SerialStub { void println(const char *) {} } Serial;
int opens = 0, reads = 0, formats = 0, mounts = 0, serializations = 0, copies = 0;
int documents = 0, jsonBuilds = 0;
bool mountResult = false, sdResult = false, sdcardMounted = false;
bool pinsFileExists = false, pinsNeedsRepair = false;
struct JsonObject {};
struct JsonDocument {
    bool empty = true;
    JsonDocument() { ++documents; }
    template<class T> T to() { empty = false; return {}; }
    template<class T> T as() { return {}; }
    bool isNull() { return empty; }
    void clear() { empty = true; }
    void remove(const char *) {}
};
struct File { bool valid; operator bool() const { return valid; } void close() {} };
struct FS {
    bool little = false;
    bool exists(const char *) { return pinsFileExists && (little ? mountResult : sdResult); }
    File open(const char *, int mode) {
        if (mode == FILE_WRITE) ++opens; else ++reads;
        return {little ? mountResult : sdResult};
    }
} SD;
struct LittleFs : FS {
    LittleFs() { little = true; }
    bool begin(bool formatOnFail, const char *, uint8_t) {
        ++mounts;
        if (formatOnFail) ++formats;
        return mountResult;
    }
    bool format() { ++formats; return true; }
    unsigned totalBytes() { return mountResult ? 40960 : 0; }
} LittleFS;
bool setupLittleFS(uint8_t maxFiles = 3);
bool setupSdCard() { sdcardMounted = sdResult; return sdResult; }
bool checkLittleFsSize() { return mountResult; }
bool getFsStorage(FS *&fs) {
    if (sdcardMounted) fs = &SD;
    else if (mountResult) fs = &LittleFS;
    else return false;
    return true;
}
bool deserializeJson(JsonDocument &doc, File &) { doc.empty = false; return false; }
const char *getMacAddress() { return "test-device"; }
void copyToFs(FS &, FS &, const char *, bool) { ++copies; }
template <class T> int serializeJsonPretty(JsonDocument &, T &) { ++serializations; return 10; }
struct FluxConfig {
    const char *filepath = "/flux.conf";
    int bright = 77, loads = 0;
    JsonDocument toJson() { return {}; }
    void fromFile(bool) { ++loads; }
    void saveFile();
} fluxConfig;
struct FluxConfigPins {
    const char *filepath = "/fluxPins.conf";
    int decoded = 0, rotation = 2;
    void toJson(JsonObject) { ++jsonBuilds; }
    void fromJson(JsonObject) { ++decoded; if (pinsNeedsRepair) saveFile(); }
    void createFile();
    void saveFile();
    void loadFile(JsonDocument &, bool checkFS = true);
    void fromFile(bool checkFS = true);
} fluxConfigPins;
'''
        source += function("src/core/sd_functions.cpp", "bool setupLittleFS(uint8_t maxFiles)")
        source += "\n" + function("src/core/config.cpp", "void FluxConfig::saveFile()")
        for signature in (
            "void FluxConfigPins::loadFile(JsonDocument &jsonDoc, bool checkFS)",
            "void FluxConfigPins::fromFile(bool checkFS)",
            "void FluxConfigPins::createFile()",
            "void FluxConfigPins::saveFile()",
        ):
            source += "\n" + function("src/core/configPins.cpp", signature)
        source += "\n" + function("src/main.cpp", "void begin_storage()")
        source += r'''
int main() {
    begin_storage();
    fluxConfig.saveFile();
    assert(formats == 0); // Never format or retry after a failed mount.
    assert(mounts == 1);
    assert(opens == 0 && serializations == 0 && copies == 0);
    assert(fluxConfig.loads == 0 && fluxConfigPins.decoded == 0);
    assert(fluxConfig.bright == 77); // Existing in-memory settings survive.
    fluxConfigPins.createFile();
    fluxConfigPins.saveFile();
    assert(opens == 0 && reads == 0 && serializations == 0 && jsonBuilds == 0);
    sdResult = true;
    int priorDocuments = documents;
    // A missing pins file on mounted SD must not create one in unmounted LittleFS.
    begin_storage();
    fluxConfig.saveFile();
    assert(fluxConfig.loads == 1 && fluxConfigPins.decoded == 0);
    assert(opens == 0 && formats == 0 && copies == 0);
    assert(documents == priorDocuments + 1 && jsonBuilds == 0);
    priorDocuments = documents;
    fluxConfigPins.createFile();
    fluxConfigPins.saveFile();
    assert(documents == priorDocuments && reads == 0 && opens == 0);
    // A partial SD pins file requests repair through saveFile, still read-only.
    pinsFileExists = true;
    pinsNeedsRepair = true;
    begin_storage();
    assert(fluxConfig.loads == 2 && fluxConfigPins.decoded == 1);
    assert(reads == 1 && opens == 0 && copies == 0 && jsonBuilds == 0);
    assert(fluxConfigPins.rotation == 2);
    mountResult = true;
    pinsNeedsRepair = false;
    begin_storage();
    fluxConfig.saveFile();
    assert(opens == 1 && copies == 1);
    assert(fluxConfig.loads == 3 && fluxConfigPins.decoded == 2);
    fluxConfigPins.createFile();
    assert(opens == 2 && copies == 2 && jsonBuilds == 1);
    fluxConfigPins.saveFile();
    assert(opens == 3 && copies == 3 && jsonBuilds == 2);
    assert(fluxConfig.bright == 77 && formats == 0);
}
'''
        with tempfile.TemporaryDirectory(prefix="flux-storage-") as directory:
            cpp = Path(directory) / "storage.cpp"
            binary = Path(directory) / ("storage.exe" if os.name == "nt" else "storage")
            cpp.write_text(source, encoding="utf-8")
            result = subprocess.run(command + ["-std=c++11", "-UNDEBUG", str(cpp), "-o", str(binary)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(binary)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
