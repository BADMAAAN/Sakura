"""Execute the real board power gesture with inert GPIO, clock and shutdown calls."""
from pathlib import Path
import subprocess
import tempfile
import unittest

from test_delta_runtime import function

ROOT = Path(__file__).resolve().parents[1]


class KeyboardPowerTests(unittest.TestCase):
    def test_actual_board_hold_release_and_shutdown(self):
        source = r'''
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>
#define T_EMBED_1101 1
#define FLUX_DELTA_BUILD 1
constexpr int BK_BTN=6, SEL_BTN=0, BTN_ACT=0, LOW=0;
constexpr int PIN_POWER_ON=15, GPIO_NUM_6=6, WIFI_OFF=0;
uint32_t now=0, releaseAt=0, bannerAt=0, blankAt=0;
bool restartMode=false, isSleeping=false, isScreenOff=false;
int tftWidth=320, tftHeight=170;
std::vector<std::string> calls;
struct String : std::string { explicit String(int n):std::string(std::to_string(n)){} };
uint32_t millis(){return now;}
void delay(unsigned ms){now+=ms;assert(now<20000);}
int digitalRead(int pin){return pin==BK_BTN ? (now<releaseAt ? 0:1) : (restartMode ? 0:1);}
struct {int bgColor=0, priColor=1, secColor=2;} fluxConfig;
struct {
 int textWidth(const char*,int){return 120;}
 int fontHeight(int){return 8;}
 void fillRect(int,int,int,int,int){if(!bannerAt)bannerAt=now;}
 void setTextSize(int){} void setTextColor(int,int){}
 void drawCentreString(const std::string&,int,int,int){}
 void fillScreen(int){calls.push_back("restart-screen");}
} tft;
void drawStatusBar(){calls.push_back("status");}
void setBrightness(int value,bool save){assert(value==0&&!save);blankAt=now;calls.push_back("backlight");}
void deltaUiInvalidate(){calls.push_back("invalidate");}
void panelSleep(bool asleep){assert(asleep&&isSleeping&&isScreenOff);calls.push_back("panel");}
void released(){assert(digitalRead(BK_BTN)!=BTN_ACT);assert(now>=releaseAt+200);}
struct {
 void disconnect(bool off,bool erase){released();assert(off&&!erase);calls.push_back("wifi-disconnect");}
 void mode(int mode){assert(mode==WIFI_OFF);calls.push_back("wifi-off");}
} WiFi;
void stopBLEStack(){released();calls.push_back("ble-stop");}
struct NimBLEDevice {static void deinit(bool clear){assert(!clear);calls.push_back("ble-deinit");}};
void ledEffects(bool on){assert(!on);calls.push_back("led-effects");}
void setLedBrightness(int n){assert(n==0);calls.push_back("led-off");}
void powerDownNFC(){released();calls.push_back("nfc");}
void powerDownCC1101(){released();calls.push_back("cc1101");}
void digitalWrite(int pin,int value){assert(pin==PIN_POWER_ON&&value==LOW);calls.push_back("power-low");}
void esp_sleep_enable_ext0_wakeup(int pin,int level){assert(pin==6&&level==LOW);calls.push_back("wake-gpio6-low");}
enum Stop {Sleep,Restart};
void esp_deep_sleep_start(){calls.push_back("deep-sleep");throw Sleep;}
struct {void restart(){calls.push_back("restart");throw Restart;}} ESP;
'''
        source += function(
            (ROOT / 'boards/lilygo-t-embed-cc1101/interface.cpp').read_text(),
            'void checkReboot()')
        source += r'''
void reset(uint32_t release,bool restart=false){
 now=0;releaseAt=release;restartMode=restart;bannerAt=blankAt=0;
 isSleeping=isScreenOff=false;calls.clear();
}
int main(){
 reset(0);checkReboot();assert(now==0&&calls.empty());
 reset(100);checkReboot();assert(now==130&&bannerAt==0&&calls.empty());
 reset(3490);checkReboot();assert(!isSleeping&&!isScreenOff&&blankAt==0);
 assert(bannerAt==510&&calls==std::vector<std::string>{"status"});
 reset(3500);checkReboot();assert(blankAt==0&&!isSleeping);
 reset(4000);
 try{checkReboot();assert(false);}catch(Stop stop){assert(stop==Sleep);}
 assert(bannerAt==510&&blankAt==3500&&now==5200&&isSleeping&&isScreenOff);
 assert((calls==std::vector<std::string>{"backlight","invalidate","panel",
  "wifi-disconnect","wifi-off","ble-stop","ble-deinit","led-effects",
  "led-off","nfc","cc1101","power-low","wake-gpio6-low","deep-sleep"}));
 reset(5500,true);checkReboot();assert(!isSleeping&&blankAt==0);
 reset(6000,true);
 try{checkReboot();assert(false);}catch(Stop stop){assert(stop==Restart);}
 assert(now==6500&&!isSleeping&&!isScreenOff&&blankAt==0);
 assert((calls==std::vector<std::string>{"restart-screen","restart"}));
}
'''
        zig = next((ROOT / '.tools/zig').rglob('zig.exe'))
        with tempfile.TemporaryDirectory(prefix='sakura-power-') as directory:
            path = Path(directory)
            (path / 'power.cpp').write_text(source)
            subprocess.run([str(zig), 'c++', '-std=c++11', '-O2', '-UNDEBUG',
                            str(path / 'power.cpp'), '-o', str(path / 'power.exe')], check=True)
            subprocess.run([str(path / 'power.exe')], check=True, timeout=10)


if __name__ == '__main__':
    unittest.main()
