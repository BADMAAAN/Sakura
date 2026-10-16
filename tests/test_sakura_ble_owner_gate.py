"""Exercise actual Suite ownership gates and menu bodies with inert BLE clients."""
from pathlib import Path
import subprocess
import tempfile
import unittest
from test_delta_runtime import function

ROOT = Path(__file__).resolve().parents[1]


class SuiteOwnerGateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        suite = (ROOT / 'src/modules/ble/BLE_Suite.cpp').read_text(encoding='utf-8')
        globals_source = (ROOT / 'include/globals.h').read_text(encoding='utf-8')
        signatures = (
            'bool BLEStateManager::ownsClient(', 'void BLEStateManager::registerClient(',
            'void BLEStateManager::unregisterClient(', 'bool BLEStateManager::releaseClient(',
            'bool BLEStateManager::cleanupAllClients(', 'size_t BLEStateManager::getActiveClientCount(',
            'static void settleOwnedSuiteClients(', 'void BleSuiteMenu(',
            'void executeAttackWithTargetScan(', 'int showSubMenu(const char *title, const char *options[], int optionCount) {')
        production = '\n'.join(function(suite, signature) for signature in signatures)
        shared = '\n'.join(function(globals_source, signature) for signature in (
            'extern inline bool check(', 'extern inline void clearInputState()'))
        cls.source = r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#define SAKURA_BUILD 1
#define T_EMBED_1101 1
#define HAS_ENCODER 1
#define pdMS_TO_TICKS(x) (x)
#define TFT_WHITE 1
#define TFT_BLACK 0
#define TFT_CYAN 2
#define TFT_GREEN 3
#define ESP_PWR_LVL_P9 9
struct String:std::string {
 using std::string::string;
 String(const std::string&s):std::string(s){}
 String(int n):std::string(std::to_string(n)){}
 bool isEmpty()const{return empty();}
 void remove(size_t at){erase(at);}
};
volatile bool EscPress=false,SelPress=false,PrevPress=false,NextPress=false,UpPress=false,DownPress=false;
volatile bool AnyKeyPress=false,SerialCmdPress=false,NextPagePress=false,PrevPagePress=false,LongPress=false;
int32_t RotaryNetSteps=0;
struct {void Clear(){}}touchPoint;
void inputDiagnosticConsumed(bool){}
SHARED
bool g_bleOperationCancelled=false,g_bleScanActive=false;
struct NimBLEClient{bool connected=true,foreign=false,deleted=false,releaseWorks=false;};
struct BLEStateManager {
 static std::vector<NimBLEClient*>activeClients;
 static bool ownsClient(NimBLEClient*);static void registerClient(NimBLEClient*);
 static void unregisterClient(NimBLEClient*);static bool releaseClient(NimBLEClient*);
 static bool cleanupAllClients();static size_t getActiveClientCount();
 static bool initBLE(const String&,int);
};
std::vector<NimBLEClient*>BLEStateManager::activeClients;
int ticks=0,errors=0,pendingScreens=0,draws=0,inits=0,operations=0,cleanupAttempts=0,globalDeinits=0;
int retryAt=5,selectAt=1,backAt=-1,normalBackAt=-1;
bool injectOwnedOnBack=false,retainAfterDispatch=false,sdkInitialized=false,exposeForeign=false,inSubmenu=false;
NimBLEClient owned,foreign;
struct Scan{bool isScanning(){return false;}void stop(){}void clearResults(){}}scan,*g_pBLEScan=nullptr;
struct Advertising{bool isAdvertising(){return false;}}advertising;
void*hid_ble=nullptr;
struct NimBLEDevice {
 static bool isInitialized(){return sdkInitialized;}
 static size_t getCreatedClientCount(){return BLEStateManager::activeClients.size()+(exposeForeign?1:0);}
 static Scan*getScan(){return &scan;}static Advertising*getAdvertising(){return &advertising;}
 static void*getServer(){return nullptr;}
 static bool deleteClient(NimBLEClient*c){assert(c&&!c->foreign&&!c->connected&&!c->deleted);c->deleted=true;return true;}
 static void deinit(){++globalDeinits;}
};
bool bleClientDisconnect(NimBLEClient*c,bool&){++cleanupAttempts;assert(!c->foreign);if(!c->releaseWorks)return false;c->connected=false;return true;}
void displayError(const char*){++errors;}
void displayRedStripe(const char*){assert(BLEStateManager::ownsClient(&owned)&&!owned.deleted);++pendingScreens;}
void waitTick(){
 assert(++ticks<50&&"Gate must finish after a real retry");
 if(ticks==selectAt)SelPress=true;
 if(ticks==backAt){if(injectOwnedOnBack)BLEStateManager::registerClient(&owned);EscPress=true;}
 if(ticks==retryAt){assert(!owned.deleted);owned.releaseWorks=true;EscPress=true;}
 if(ticks==normalBackAt)EscPress=true;
}
void delay(int){waitTick();}void vTaskDelay(int){waitTick();}
bool BLEStateManager::initBLE(const String&,int){assert(activeClients.empty());++inits;return true;}
int tftWidth=320,tftHeight=170;
struct {int bgColor=0;}fluxConfig;
struct Tft{
 void fillScreen(int){assert(inSubmenu||BLEStateManager::activeClients.empty());++draws;}
 template<class...T>void drawRect(T...){}template<class...T>void fillRect(T...){}
 template<class...T>void setTextColor(T...){}template<class...T>void setTextSize(T...){}
 template<class...T>void setTextWrap(T...){}template<class...T>void setCursor(T...){}
 template<class T>void print(T){}int textWidth(const char*s){return int(strlen(s));}
}tft;
void TouchFooter(){}void showWelcomeScreen(){assert(BLEStateManager::activeClients.empty());}
struct {void clear(){assert(BLEStateManager::activeClients.empty());}}scannerData;
struct SelectedDevice{String address,name;}g_selectedDevice;
struct NimBLEAddress{String toString(){return "fake";}};
const char*getScanTitle(int){return "fake";}
String selectTargetFromScan(const char*){
 assert(BLEStateManager::activeClients.empty());++operations;
 if(retainAfterDispatch){BLEStateManager::registerClient(&owned);g_bleOperationCancelled=true;}
 return "";
}
NimBLEAddress parseAddress(const String&){return {};}
bool confirmAttack(const char*){assert(false&&"No target was selected");return false;}
void executeAttackWithTargetScan(int);
void BLE_Sniffer(){assert(BLEStateManager::activeClients.empty());++operations;}
void showAttackProgress(const char*,int){}
#define INERT_ACTION(name) void name(NimBLEAddress,SelectedDevice){assert(false&&"Unexpected active dispatch");}
INERT_ACTION(runQuickTest) INERT_ACTION(runDeviceProfiling) INERT_ACTION(showFastPairSubMenu)
INERT_ACTION(showHFPSubMenu) INERT_ACTION(showAudioSubMenu) INERT_ACTION(showHIDSubMenu)
INERT_ACTION(showMemorySubMenu) INERT_ACTION(showDoSSubMenu) INERT_ACTION(showPayloadSubMenu)
INERT_ACTION(showTestingSubMenu) INERT_ACTION(runUniversalAttack)
PRODUCTION
int main(int argc,char**argv){
 assert(argc==2);std::string scenario=argv[1];foreign.foreign=true;foreign.releaseWorks=false;
 if(scenario=="settle"||scenario=="entry"||scenario=="dispatch-entry"){
  BLEStateManager::registerClient(&owned);
  if(scenario=="settle")settleOwnedSuiteClients();
  else if(scenario=="entry")BleSuiteMenu();else executeAttackWithTargetScan(0);
  assert(ticks>=5&&owned.deleted&&BLEStateManager::activeClients.empty());
  assert(errors==1&&pendingScreens==1&&cleanupAttempts==2&&g_bleOperationCancelled);
  assert(inits==0&&draws==0&&operations==0&&!SelPress&&!EscPress);
  if(scenario=="entry"){
   retryAt=selectAt=-1;EscPress=true;BleSuiteMenu();
   assert(inits==1&&draws>0&&!g_bleOperationCancelled&&operations==0);
  }
 }else if(scenario=="menu-back"){
  injectOwnedOnBack=true;backAt=1;selectAt=-1;BleSuiteMenu();
  assert(ticks>=5&&owned.deleted&&BLEStateManager::activeClients.empty());
  assert(inits==1&&operations==0&&draws==1&&cleanupAttempts==2);
 }else if(scenario=="after-dispatch"){
  retainAfterDispatch=true;normalBackAt=6;SelPress=true;BleSuiteMenu();
  assert(ticks>=6&&owned.deleted&&BLEStateManager::activeClients.empty());
  assert(inits==2&&operations==1&&cleanupAttempts==2&&draws==2);
 }else if(scenario=="submenu"){
  inSubmenu=true;BLEStateManager::registerClient(&owned);g_bleOperationCancelled=true;SelPress=true;
  const char*items[]={"safe fake"};assert(showSubMenu("Nested",items,1)==-1);
  assert(g_bleOperationCancelled&&BLEStateManager::ownsClient(&owned)&&!owned.deleted&&cleanupAttempts==0);
 }else if(scenario=="foreign"){
  sdkInitialized=exposeForeign=true;foreign.connected=false;BleSuiteMenu();
  assert(errors==1&&inits==0&&draws==0&&operations==0&&cleanupAttempts==0);
 }else assert(false);
 assert(!foreign.deleted&&globalDeinits==0);
}
'''.replace('SHARED', shared).replace('PRODUCTION', production)
        cls.directory = tempfile.TemporaryDirectory(prefix='sakura-suite-owner-gate-')
        cls.exe = Path(cls.directory.name) / 'gate.exe'
        cls.zig = next((ROOT / '.tools/zig').rglob('zig.exe'))
        cls.build(cls.source, cls.exe)

    @classmethod
    def build(cls, source, exe):
        result = subprocess.run([str(cls.zig), 'c++', '-std=c++11', '-O2', '-UNDEBUG',
                                 '-x', 'c++', '-', '-o', str(exe)],
                                input=source, text=True, capture_output=True, timeout=120)
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)

    @classmethod
    def tearDownClass(cls):
        cls.directory.cleanup()

    def run_case(self, scenario):
        result = subprocess.run([str(self.exe), scenario], capture_output=True, text=True, timeout=5)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_failed_release_waits_for_late_retry(self):
        self.run_case('settle')

    def test_retained_entry_returns_then_fresh_entry_works(self):
        self.run_case('entry')

    def test_retained_action_cannot_start_target_scan(self):
        self.run_case('dispatch-entry')

    def test_main_back_finishes_cleanup_before_parent(self):
        self.run_case('menu-back')

    def test_after_action_cleanup_consumes_select_before_redraw(self):
        self.run_case('after-dispatch')

    def test_cancelled_nested_menu_preserves_parent_client(self):
        self.run_case('submenu')

    def test_pending_foreign_client_prevents_suite_entry(self):
        self.run_case('foreign')

    def test_old_one_shot_cleanup_behavior_is_rejected(self):
        original = function(self.source, 'static void settleOwnedSuiteClients(')
        broken = 'static void settleOwnedSuiteClients(){BLEStateManager::cleanupAllClients();clearInputState();}'
        exe = Path(self.directory.name) / 'old-gate.exe'
        self.build(self.source.replace(original, broken), exe)
        result = subprocess.run([str(exe), 'entry'], capture_output=True, text=True, timeout=5)
        self.assertNotEqual(result.returncode, 0, 'One-shot failed cleanup unexpectedly passed')
        self.assertIn('Assertion failed', result.stderr)


if __name__ == '__main__':
    unittest.main()
