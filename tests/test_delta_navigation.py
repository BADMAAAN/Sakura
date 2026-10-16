"""Run the real menu loop with inert display and input stubs; no radio actions."""
from pathlib import Path
import subprocess
import tempfile
import unittest
from test_delta_runtime import function

ROOT = Path(__file__).resolve().parents[1]


class DeltaNavigationTests(unittest.TestCase):
    def test_actual_menu_callbacks_and_encoder(self):
        text = (ROOT / "src/core/display.cpp").read_text(encoding="utf-8")
        start = text.index("int loopOptions(")
        opening = text.index("{", start)
        end, depth = opening + 1, 1
        while depth:
            depth += (text[end] == "{") - (text[end] == "}")
            end += 1
        source = r'''
#include <cassert>
#include <algorithm>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#define FLUX_DELTA_BUILD 1
#define HAS_ENCODER 1
#define SAKURA_BUILD 1
#define T_EMBED_1101 1
#define MENU_TYPE_MAIN 0
#define MENU_TYPE_SUBMENU 1
#define MENU_TYPE_REGULAR 2
#define MAX_MENU_SIZE 5
#define pdMS_TO_TICKS(x) (x)
using String=std::string;
struct Option {String label;std::function<void()> operation;bool enabled=true,hovered=false;
 std::function<bool(void*,bool)>hover;void*hoverPointer=nullptr;
 Option(String l,std::function<void()>fn,bool e=true):label(l),operation(fn),enabled(e){} };
struct Opt_Coord{};
struct Config{bool devMode=false;uint16_t priColor=1,secColor=2,bgColor=0;void setDevMode(bool b){devMode=b;}}fluxConfig;
struct SerialStub{template<class T>void print(T){}template<class T>void println(T){}}Serial;
struct MainMenu{void setMainMoveDirection(int){}}mainMenu;
volatile bool PrevPress=false,NextPress=false,UpPress=false,DownPress=false,SelPress=false,EscPress=false,AnyKeyPress=false,SerialCmdPress=false;
bool cancelOnDelay=false,selectOnDelay=false;
int forceMenuOption=-1,interpreter_state=0,menuOptionType=0,ends=0,delays=0,draws=0;
int32_t RotaryNetSteps=0;
String menuOptionLabel;
uint32_t clockMs=0;
uint32_t millis(){return clockMs+=1000;}
void inputDiagnosticConsumed(bool){}
void inputDiagnosticScreen(const char*){}
void inputDiagnosticAction(const char*,const char*){}
/* PRODUCTION_CHECK */
int32_t drainRotarySteps(){int32_t result=RotaryNetSteps;RotaryNetSteps=0;return result;}
void taskYIELD(){}
void vTaskDelay(int){assert(++delays<50);if(cancelOnDelay)EscPress=true;if(selectOnDelay)SelPress=true;}
void drawMainBorder(){}
void checkReboot(){}
void displayInfo(const char*,bool){}
void drawSubmenu(int,std::vector<Option>&,const char*,int,bool){++draws;}
Opt_Coord drawOptions(int,std::vector<Option>&,int,int,int,bool){++draws;return {};}
void displayScrollingText(const String&,Opt_Coord&,bool){}
void deltaUiTick(){}
void deltaUiMenuTick(){}
int inputDispatches=0;void deltaUiInputConsumed(){++inputDispatches;}
void deltaUiMenuEnd(){++ends;}
void sakuraRecordMenu(const char*,const char*){}
int loopOptions(std::vector<Option>&,uint8_t=MENU_TYPE_REGULAR,const char*="",int=0,bool=false);
'''
        # Keep the tested implementation byte-for-byte, apart from its standalone declaration.
        source = source.replace('/* PRODUCTION_CHECK */', function(
            (ROOT / 'include/globals.h').read_text(encoding='utf-8'), 'extern inline bool check('))
        source = source.replace('const char*=""', 'const char* = ""') + text[start:end]
        source += r'''
void reset(){PrevPress=NextPress=UpPress=DownPress=SelPress=EscPress=false;forceMenuOption=-1;
 RotaryNetSteps=0;delays=0;cancelOnDelay=selectOnDelay=false;interpreter_state=0;inputDispatches=0;}
int main(){
 std::vector<Option> empty;
 assert(loopOptions(empty)==-1);
 int called=-1;
 std::vector<Option> rows={{"off",[&]{called=0;},false},{"one",[&]{called=1;}},{"two",[&]{called=2;}}};
 reset();SelPress=true;assert(loopOptions(rows)==1&&called==1);
 rows[1].enabled=rows[2].enabled=false;reset();assert(loopOptions(rows)==-1);
 rows[1].enabled=rows[2].enabled=true;
 reset();forceMenuOption=2;called=-1;int before=ends;
 assert(loopOptions(rows,MENU_TYPE_SUBMENU,"Nested",1)==1&&called==2&&ends>before);
 reset();EscPress=true;called=-1;assert(loopOptions(rows)==-1&&called==-1);
 reset();EscPress=true;selectOnDelay=true;called=-1;
 assert(loopOptions(rows,MENU_TYPE_MAIN,"Home",1)==1&&called==1&&!EscPress);
 reset();forceMenuOption=0;cancelOnDelay=true;called=-1;
 assert(loopOptions(rows)==-1&&called==-1); // Disabled forced target never runs.
 reset();RotaryNetSteps=-10001;SelPress=true;called=-1;
 assert(loopOptions(rows,MENU_TYPE_SUBMENU,"Burst",1)==2&&called==2);assert(inputDispatches==1); // Skips disabled, wraps.
 reset();RotaryNetSteps=10002;SelPress=true;called=-1;
 assert(loopOptions(rows,MENU_TYPE_SUBMENU,"Reverse",2)==2&&called==2);
 reset();RotaryNetSteps=INT32_MIN;SelPress=true;called=-1;
 assert(loopOptions(rows,MENU_TYPE_SUBMENU,"Minimum",1)==1&&called==1);
 reset();RotaryNetSteps=INT32_MAX;SelPress=true;called=-1;
 assert(loopOptions(rows,MENU_TYPE_SUBMENU,"Maximum",1)==2&&called==2);
 const char*names[]={"WiFi","BLE","RF","LoRa","NRF24","FM","IR","Ethernet","GPS","RFID","Files","JS Interpreter","Clock","Others","Config","Favorites"};
 const int physicalIndices[]={0,1,2,3,6,7,9,10,11,12,13,14,15};
 for(int total:{13,16}){
  std::vector<Option> main;
  for(int i=0;i<total;++i)main.emplace_back(names[total==13?physicalIndices[i]:i],[&,i]{called=i;});
  for(int i=0;i<total;++i){reset();forceMenuOption=i;called=-1;
   assert(loopOptions(main,MENU_TYPE_MAIN,"Main Menu",0)==0&&called==i);}
 }
 reset();called=-1;rows[2].operation=[&]{
  std::vector<Option> nested={{"Return",[&]{called=42;}}};forceMenuOption=0;
  assert(loopOptions(nested,MENU_TYPE_SUBMENU,"Child",0)==0);
 };
 forceMenuOption=2;
 assert(loopOptions(rows,MENU_TYPE_SUBMENU,"Parent",1)==1&&called==42);
 assert(RotaryNetSteps==0&&draws>0&&inputDispatches==0);
}
'''
        compiler = next((ROOT / ".tools/zig").rglob("zig.exe"))
        with tempfile.TemporaryDirectory(prefix="delta-navigation-") as temp:
            cpp = Path(temp) / "navigation.cpp"
            cpp.write_text(source, encoding="utf-8")
            exe = Path(temp) / "navigation.exe"
            result = subprocess.run([str(compiler), "c++", "-std=c++11", "-O2", "-UNDEBUG",
                                     str(cpp), "-o", str(exe)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
