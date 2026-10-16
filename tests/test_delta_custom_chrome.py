"""Run the actual bespoke BLE painters and device-info formatting without radios."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def function(text, signature):
    start = text.index(signature)
    opening = text.index('{', start)
    end, depth = opening + 1, 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


class DeltaCustomChromeTests(unittest.TestCase):
    def test_actual_painters_and_device_info(self):
        ble = (ROOT / 'src/modules/ble/BLE_Suite.cpp').read_text(encoding='utf-8')
        utils = (ROOT / 'src/core/utils.cpp').read_text(encoding='utf-8')
        device = function(utils, 'void showDeviceInfo()')
        storage = device[device.index('    const uint64_t littleTotal'):device.index('\n#ifdef HAS_SCREEN')]
        battery = device[device.index('    const int battery = getBattery();'):device.index('\n#ifdef USE_BQ27220_VIA_I2C')]
        target = function(ble, 'String selectTargetFromScan(const char *title)')
        cache_start = target.index('    // Snapshot acquisition/sorting has finished;')
        cache = target[cache_start:target.index('#endif', cache_start)]
        source = r'''
#include <cassert>
#include <string>
#include <vector>
#include <algorithm>
#include <delta/instruments.h>
#define TFT_RED 1
#define TFT_YELLOW 2
#define TFT_ORANGE 3
#define TFT_GREEN 4
uint16_t pixels[320*170+2];
delta::Appearance appearance;delta::Status status;
bool available=true;int presents=0;
uint16_t*deltaUiCanvas(bool clear=true,bool decoration=true){
    if(!available)return nullptr;
    if(clear)delta::ui::workBackground(pixels+1,appearance,decoration);
    return pixels+1;
}
void deltaUiPresent(){++presents;}
const delta::Appearance&deltaUiAppearance(){return appearance;}
const delta::Status&deltaUiStatus(){return status;}
class String: public std::string {
public:
    using std::string::string;
    String(const std::string&s):std::string(s){}
    String(int n):std::string(std::to_string(n)){}
};
String formatBytes(uint64_t bytes){return std::to_string(bytes);}
struct Storage{
    uint64_t total=0,used=0;int totalCalls=0,usedCalls=0;
    uint64_t totalBytes(){++totalCalls;return total;}
    uint64_t usedBytes(){++usedCalls;return used;}
}LittleFS,SD;
bool sdcardMounted=false;
int batteryValue=-1;int getBattery(){return batteryValue;}
struct Area{std::vector<String>lines;void addLine(const String&s){lines.push_back(s);}};
struct DeviceSnapshot{
    std::vector<String>names,addresses;std::vector<int>rssi;
    std::vector<bool>fastPair,hfp;std::vector<uint8_t>types;
};
'''
        source += function(ble, 'static bool deltaSuiteMenu(')
        source += function(ble, 'static bool deltaSuitePanel(')
        source += function(ble, 'static bool deltaSuiteDevices(')
        source += '\nArea storageInfo(){Area area;\n' + storage + '\nreturn area;}\n'
        source += '\nArea batteryInfo(){Area area;\n' + battery + '\nreturn area;}\n'
        source += r'''
uint64_t hash(){uint64_t h=0;for(int i=1;i<=320*170;++i)h=h*33+pixels[i];return h;}
int main(){
    const char*items[]={"Quick scan","Device profiling","FastPair","HFP","Audio","HID","Memory","DoS","Payload","Testing","Universal","Sniffer"};
    pixels[0]=pixels[320*170+1]=0xabcd;
    for(int theme=0;theme<3;++theme){
        appearance.theme=delta::Theme(theme);
        assert(deltaSuiteMenu("BLE SUITE",nullptr,0,0));
        assert(deltaSuiteMenu("BLE SUITE",items,12,-1));
        uint64_t first=hash();
        assert(deltaSuiteMenu("BLE SUITE",items,12,11));assert(hash()!=first);
        assert(deltaSuitePanel("MESSAGE",nullptr,"Waiting",TFT_GREEN));
        assert(deltaSuitePanel("CONFIRM","Read-only fixture","",TFT_ORANGE,"Yes","No","Details"));
        assert(deltaSuitePanel("CONFIRM","Read-only fixture","",TFT_ORANGE,nullptr,"No",nullptr));
        const std::string longMessage(4096,'W');
        assert(deltaSuitePanel("ERROR",longMessage.c_str(),"Back",TFT_RED));
        assert(pixels[0]==0xabcd&&pixels[320*170+1]==0xabcd);
    }
    DeviceSnapshot sample;
    for(int i=0;i<7;++i){sample.names.push_back("Real fixture "+std::to_string(i));
        sample.addresses.push_back("AA:BB:CC:DD:EE:0"+std::to_string(i));
        sample.rssi.push_back(i==6?0:-40-i*7);sample.fastPair.push_back(i==0);
        sample.hfp.push_back(i==1);sample.types.push_back(i==2?3:0);}
    DeviceSnapshot*snapshot=&sample;
'''
        source += cache
        source += r'''
    assert(deltaRows.size()==7&&deltaRows[0].metric==-40&&deltaRows[5].metric==-75);
    assert(deltaRows[6].metric==delta::ui::Unmeasured);
    assert(std::string(deltaRows[0].detail)=="AA:BB:CC:DD:EE:00 FP");
    assert(std::string(deltaRows[1].detail)=="AA:BB:CC:DD:EE:01 HFP");
    assert(std::string(deltaRows[2].detail)=="AA:BB:CC:DD:EE:02 AUDIO HID");
    const auto rowStorage=deltaRows.data();const size_t rowCapacity=deltaRows.capacity();
    std::vector<bool>checked(7,false);
    assert(deltaSuiteDevices("SELECT TARGET",deltaRows,0));uint64_t plain=hash();
    checked[0]=true;assert(deltaSuiteDevices("SELECT TARGETS",deltaRows,0,&checked,1));
    assert(hash()!=plain);
    for(int i=0;i<50;++i){assert(deltaSuiteDevices("SELECT TARGETS",deltaRows,i%7,&checked,1));
        assert(deltaRows.data()==rowStorage&&deltaRows.capacity()==rowCapacity);
        assert(pixels[0]==0xabcd&&pixels[320*170+1]==0xabcd);}
    const std::vector<delta::ui::RadioRow>empty;
    assert(deltaSuiteDevices("EMPTY",empty,99));
    available=false;assert(!deltaSuiteMenu("",items,12,0));assert(!deltaSuitePanel("","","",TFT_RED));
    assert(!deltaSuiteDevices("",deltaRows,0));
    LittleFS.total=0;LittleFS.used=123;SD.total=123;SD.used=9;
    auto info=storageInfo();
    assert(info.lines[0]=="LittleFS: not mounted"&&LittleFS.usedCalls==0);
    assert(SD.totalCalls==0&&SD.usedCalls==0);
    LittleFS.total=100;LittleFS.used=120;sdcardMounted=true;SD.total=100;SD.used=150;
    info=storageInfo();
    assert(info.lines[2]=="LittleFS free: 0"&&info.lines[6]=="SD Card free: 0");
    LittleFS.used=30;SD.used=20;info=storageInfo();
    assert(info.lines[2]=="LittleFS free: 70"&&info.lines[6]=="SD Card free: 80");
    batteryValue=-1;assert(batteryInfo().lines[0]=="Charge: Unknown");
    batteryValue=101;assert(batteryInfo().lines[0]=="Charge: Unknown");
    batteryValue=0;assert(batteryInfo().lines[0]=="Charge: 0%");
    batteryValue=100;assert(batteryInfo().lines[0]=="Charge: 100%");
}
'''
        compiler = next((ROOT / '.tools/zig').rglob('zig.exe'))
        with tempfile.TemporaryDirectory(prefix='delta-chrome-') as temp:
            cpp = Path(temp) / 'chrome.cpp'
            exe = Path(temp) / 'chrome.exe'
            cpp.write_text(source, encoding='utf-8')
            result = subprocess.run([str(compiler), 'c++', '-std=c++11', '-O2', '-UNDEBUG',
                                     '-I', str(ROOT / 'include'), str(cpp), '-o', str(exe)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_firmware_routes_reach_shared_painters(self):
        ble = (ROOT / 'src/modules/ble/BLE_Suite.cpp').read_text(encoding='utf-8')
        for name in ('BleSuiteMenu()', 'showSubMenu(const char *title,', 'showWelcomeScreen()',
                     'showAttackProgress(', 'showAttackResult(', 'confirmAttack(',
                     'requireSimpleConfirmation(', 'showAdaptiveMessage(',
                     'showWarningMessage(', 'showErrorMessage(', 'showSuccessMessage(',
                     'selectFileFromSD()', 'selectTargetFromScan(const char *title)',
                     'selectMultipleTargetsFromScan('):
            # Locate definitions, skipping forward declarations and callers.
            import re
            match = re.search(r'^(?:void|bool|int|int8_t|String) ' + re.escape(name), ble, re.M)
            self.assertIsNotNone(match, name)
            signature = ble[match.start():ble.index('{', match.start())]
            if ';' in signature:  # showSubMenu has one forward declaration.
                match = re.search(r'^int showSubMenu\([^;{]+\) \{', ble, re.M)
            body = function(ble[match.start():], name)
            self.assertIn('deltaSuite', body, name)
        self.assertIn('ScrollableTextArea area(title);', function(ble, 'void showDeviceInfoScreen('))
        target = function(ble, 'String selectTargetFromScan(const char *title)')
        self.assertIn('Waiting for real results', target)
        self.assertIn('deltaSuitePanel("NO DEVICES"', target)
        for signature in ('String selectTargetFromScan(const char *title)',
                          'String selectMultipleTargetsFromScan('):
            body = function(ble, signature)
            loop = body.index('while (!exit')
            self.assertLess(body.index('deltaRows.reserve('), loop)
            self.assertNotIn('deltaRows.emplace_back', body[loop:])


if __name__ == '__main__':
    unittest.main()
