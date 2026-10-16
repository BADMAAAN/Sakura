"""Execute production Wi-Fi cancellation bodies with inert SDK and UI doubles."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def body(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[opening:end]


class WifiCancellationTests(unittest.TestCase):
    def test_production_flow(self):
        wifi = (ROOT / 'src/core/wifi/wifi_common.cpp').read_text(encoding='utf-8')
        declarations = '''
static bool wifiBack(bool foreground);
static bool wifiForeignOperation();
static bool ensureOwnedWifiEvents();
static void stopOwnedWifiAttempt(bool foreground);
static int scanOwnedWifi(bool foreground, WifiConnectOutcome &outcome);
bool _connectToWifiNetwork(const String &ssid, const String &pwd, WifiConnectOutcome *outcome = nullptr, bool foreground = true);
bool _wifiConnect(const String &ssid, int encryption, WifiConnectOutcome *outcome = nullptr);
static bool connectKnownWifi(bool foreground, WifiConnectOutcome *outcome);
'''
        production = 'struct WifiOperationGuard ' + body(wifi, 'struct WifiOperationGuard') + ';\n'
        for declaration in declarations.strip().splitlines():
            signature = declaration.removesuffix(';')
            lookup = signature[:signature.index('(') + 1]
            production += signature.replace(' = nullptr', '').replace(' = true', '') + ' ' + body(wifi, lookup) + '\n'
        harness = HARNESS.replace('/* DECLARATIONS */', declarations).replace('/* PRODUCTION */', production)
        with tempfile.TemporaryDirectory(prefix='wifi-cancel-') as directory:
            exe = Path(directory) / 'check.exe'
            zig = next((ROOT / '.tools/zig').rglob('zig.exe'))
            result = subprocess.run([str(zig), 'c++', '-std=c++11', '-O1', '-UNDEBUG', '-x', 'c++', '-', '-o', str(exe)],
                                    input=harness, text=True, capture_output=True, cwd=ROOT)
            self.assertEqual(result.returncode, 0, result.stderr)
            result = subprocess.run([str(exe)], text=True, capture_output=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stderr)

    def test_no_cancel_continuations(self):
        wifi = (ROOT / 'src/core/wifi/wifi_common.cpp').read_text(encoding='utf-8')
        startup = (ROOT / 'src/core/startup_app.cpp').read_text(encoding='utf-8')
        serial = (ROOT / 'src/core/serial_commands/wifi_commands.cpp').read_text(encoding='utf-8')
        web = (ROOT / 'src/core/wifi/webInterface.cpp').read_text(encoding='utf-8')
        menu = body(wifi, 'bool wifiConnectMenu(')
        self.assertIn('_wifiConnect(__ssid.c_str(), 8, &result)', menu)
        self.assertIn('_wifiConnect(selSsid, selEnc, &result)', menu)
        self.assertNotIn('wifiDisconnect()', menu)
        self.assertNotIn('scanNetworks()', wifi)
        self.assertNotIn('wifiDisconnect()', body(wifi, 'bool _wifiConnect('))
        for source, marker in ((startup, '_startupApps["WebUI"] = []()'), (serial, '} else if (status == "on")')):
            flow = body(source, marker)
            self.assertLess(flow.index('WifiConnectOutcome::Cancelled'), flow.index('startWebUi(') if 'startWebUi(' in flow else flow.index('wifiConnectMenu(WIFI_AP)'))
            self.assertIn('WifiConnectOutcome::Busy', flow)
        self.assertIn('if (!connected) return;', body(web, 'void startWebUi('))
        task = body(wifi, 'void wifiConnectTask(')
        self.assertLess(task.index('connectKnownWifi(false, nullptr)'), task.index('vTaskDelete'))


HARNESS = r'''
#include <cassert>
#include <atomic>
#include <cstdint>
#include <climits>
#include <string>
#include <functional>
#include <vector>
#include <utility>
struct String : std::string { using std::string::string; String(const std::string &s): std::string(s) {} String()=default; };
enum class WifiConnectOutcome : uint8_t { Connected, Failed, Cancelled, Busy };
enum { WIFI_MODE_STA=1, WIFI_MODE_AP=2, WIFI_SCANNING_BIT=1, WIFI_SCAN_RUNNING=-1, WIFI_SCAN_FAILED=-2, ESP_OK=0, ARDUINO_EVENT_WIFI_SCAN_DONE=1 };
using arduino_event_id_t = int; using esp_err_t = int;
static std::atomic_flag wifiOperation = ATOMIC_FLAG_INIT;
static std::atomic<uint32_t> wifiScanDoneSequence{0};
static bool wifiTransitioning, wifiConnected, returnToMenu, EscPress, isWebUIActive;
static String wifiIP;
static uint32_t nowMs, cancelAt=UINT_MAX, connectAt=UINT_MAX, scanDoneAt, scanDuration=40;
static bool backConsumed, scanActive, stopOk=true, foreignConnected;
static int begins, disconnects, disables, disableFailures, disableStaleModes, stripes, stops, scanDeletes, scanStarts, checks, retries, saves, timezoneStarts, stopFailures, modeValue=1;
static void (*eventCallback)(int);
uint32_t millis(){ return nowMs; }
void vTaskDelay(uint32_t ms){ assert(ms <= 20); nowMs+=ms; if(scanActive && nowMs>=scanDoneAt){scanActive=false; eventCallback(1);} }
#define pdMS_TO_TICKS(x) (x)
#define FORCE_RADIO_TEARDOWN_ON_SWITCH false
#define RAM_LOG(x)
bool check(bool &){ ++checks; if(!backConsumed && nowMs>=cancelAt){backConsumed=true;return true;} return false; }
void displayError(const String &, bool=true){}
void displayWarning(const String &){}
void displayTextLine(const String &){}
void displayRedStripe(const String &){++stripes;}
void drawMainBorderWithTitle(const char *){}
void padprintln(const char *){}
void padprint(const String &){}
void stopBLEStack(){}
bool radioHasMemForWifi(){return true;}
bool BLEConnected=false;
void wakeUpScreen(){}
struct Option { String label; std::function<void()> operation; Option(const char *s,std::function<void()> fn):label(s),operation(fn){} };
std::vector<Option> options;
int loopOptions(std::vector<Option>&){++retries; return -1;}
String keyboard(const String &,int,const char *,bool){return "password";}
struct Config { String getWifiPassword(const String &){return "password";} void addWifiCredential(const String &,const String &){++saves;} } fluxConfig;
void *timezoneTaskHandle=nullptr;
void updateTimezoneTask(void *){}
void xTaskCreate(void (*)(void *),const char *,int,void *,int,void **h){++timezoneStarts;*h=(void *)1;}
struct IP { String toString(){return "127.0.0.1";} };
struct WifiDouble {
    bool isConnected(){return foreignConnected || (begun && nowMs>=connectAt);}
    bool begun=false;
    int getMode(){return modeValue;}
    int getStatusBits(){return scanActive ? WIFI_SCANNING_BIT : 0;}
    bool mode(int value){modeValue=value;return true;}
    void begin(const String &,const String &){++begins;begun=true;}
    bool disconnect(bool off,bool erase,int timeout){assert(!off&&!erase&&!timeout);++disconnects;if(stopFailures-->0)return false;begun=false;return true;} // Deliberately NO disconnect event.
    bool enableSTA(bool enable){assert(!enable);++disables;if(disableFailures-->0)return false;if(disableStaleModes-->0)return true;modeValue&=~WIFI_MODE_STA;begun=false;return true;}
    int onEvent(void (*cb)(int),int){eventCallback=cb;return 1;}
    int scanNetworks(bool async){assert(async);++scanStarts;scanActive=true;scanDoneAt=nowMs+scanDuration;return WIFI_SCAN_RUNNING;}
    int scanComplete(){assert(!scanActive);return 2;}
    void scanDelete(){assert(!scanActive);++scanDeletes;}
    String SSID(int i){return i ? "B" : "A";}
    IP localIP(){return {};}
} WiFi;
int esp_wifi_scan_stop(){++stops;if(stopOk)scanDoneAt=nowMs+20;return stopOk ? ESP_OK : -9;}
/* DECLARATIONS */
/* PRODUCTION */
void reset(){nowMs=0;cancelAt=connectAt=UINT_MAX;scanDuration=40;backConsumed=false;scanActive=false;stopOk=true;foreignConnected=false;WiFi.begun=false;wifiConnected=false;returnToMenu=false;isWebUIActive=false;modeValue=1;begins=disconnects=disables=disableFailures=disableStaleModes=stripes=stops=scanDeletes=scanStarts=checks=retries=saves=timezoneStarts=stopFailures=0;timezoneTaskHandle=nullptr;}
int main(){
    WifiConnectOutcome result;
    reset();cancelAt=0;assert(!_connectToWifiNetwork("A","p",&result));assert(result==WifiConnectOutcome::Cancelled&&begins==0&&disconnects==0);
    reset();cancelAt=100;assert(!_connectToWifiNetwork("A","p",&result));assert(result==WifiConnectOutcome::Cancelled&&nowMs==100&&disconnects==1&&disables==1&&!(modeValue&WIFI_MODE_STA));
    reset();cancelAt=connectAt=100;assert(!_connectToWifiNetwork("A","p",&result));assert(result==WifiConnectOutcome::Cancelled&&disconnects==1);
    reset();connectAt=100;assert(_connectToWifiNetwork("A","p",&result));assert(result==WifiConnectOutcome::Connected&&disconnects==0);
    reset();modeValue=WIFI_MODE_AP;assert(!_connectToWifiNetwork("A","p",&result));assert(result==WifiConnectOutcome::Busy&&begins==0&&disconnects==0);
    reset();foreignConnected=true;assert(!_connectToWifiNetwork("A","p",&result));assert(result==WifiConnectOutcome::Busy&&disconnects==0);
    reset();scanActive=true;assert(scanOwnedWifi(true,result)<0);assert(result==WifiConnectOutcome::Busy&&scanStarts==0&&stops==0);
    reset();cancelAt=0;assert(scanOwnedWifi(true,result)<0);assert(result==WifiConnectOutcome::Cancelled&&scanStarts==0);
    reset();cancelAt=20;assert(scanOwnedWifi(true,result)<0);assert(result==WifiConnectOutcome::Cancelled&&stops==1&&scanDeletes==1&&nowMs==40);
    reset();cancelAt=scanDuration=40;assert(scanOwnedWifi(true,result)<0);assert(result==WifiConnectOutcome::Cancelled&&stops==0&&scanDeletes==1);
    reset();cancelAt=20;stopOk=false;scanDuration=100;assert(scanOwnedWifi(true,result)<0);assert(nowMs==100&&scanDeletes==1&&stops==1);
    reset();scanDuration=16000;assert(scanOwnedWifi(true,result)<0);assert(result==WifiConnectOutcome::Failed&&stops==1&&nowMs==15020);
    reset();cancelAt=100;assert(!_wifiConnect("A",1,&result));assert(result==WifiConnectOutcome::Cancelled&&retries==0&&saves==0&&timezoneStarts==0);
    reset();cancelAt=100;assert(!connectKnownWifi(true,&result));assert(result==WifiConnectOutcome::Cancelled&&begins==1&&timezoneStarts==0);
    reset();cancelAt=0;connectAt=100;assert(connectKnownWifi(false,&result));assert(checks==0&&timezoneStarts==1); // Background never consumes Back.
    reset();{WifiOperationGuard held;assert(held.acquired);assert(!connectKnownWifi(true,&result));assert(result==WifiConnectOutcome::Busy&&scanStarts==0);} {WifiOperationGuard released;assert(released.acquired);}
    reset();cancelAt=20;stopFailures=1;assert(!_connectToWifiNetwork("A","p",&result));assert(disconnects==1&&disables==1&&!(modeValue&WIFI_MODE_STA));
    reset();cancelAt=20;disableFailures=2;assert(!_connectToWifiNetwork("A","p",&result));assert(disables==3&&nowMs>=540&&stripes==2&&!(modeValue&WIFI_MODE_STA));
    reset();cancelAt=20;disableStaleModes=1;assert(!_connectToWifiNetwork("A","p",&result));assert(disables==2&&nowMs>=280&&!(modeValue&WIFI_MODE_STA));
    reset();isWebUIActive=true;assert(!connectKnownWifi(true,&result));assert(result==WifiConnectOutcome::Busy&&begins==0&&scanStarts==0&&disables==0);
    reset();isWebUIActive=true;assert(!_connectToWifiNetwork("A","p",&result));assert(result==WifiConnectOutcome::Busy&&disconnects==0&&disables==0);
    reset();modeValue=WIFI_MODE_STA|WIFI_MODE_AP;wifiConnected=true;stopOwnedWifiAttempt(true);assert(modeValue==WIFI_MODE_AP&&wifiConnected&&disables==1); // AP survives owned STA stop.
}
'''


if __name__ == '__main__':
    unittest.main()
