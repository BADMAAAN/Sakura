"""Execute actual canvas/cache entry points with a tiny host TFT, without device I/O."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def function(text, signature):
    start = text.index(signature)
    opening = text.index("{", start)
    end, depth = opening + 1, 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


class DeltaRuntimeTests(unittest.TestCase):
    def test_actual_canvas_menu_cache_and_sleep(self):
        production = (ROOT / "src/core/delta_ui.cpp").read_text(encoding="utf-8")
        declarations = production[production.index("struct WorkRow"):production.index("const char *const sectionNames")]
        source = r'''
#include <cassert>
#include <string>
#include <vector>
#include <algorithm>
#include "delta/instruments.h"
#include "delta/dirty.h"
struct String : std::string {
 using std::string::string;
 String(const std::string&s):std::string(s){}
 bool isEmpty()const{return empty();}
 int indexOf(const char*s)const{auto i=find(s);return i==npos?-1:int(i);}
 String substring(int i)const{return substr(i);}
 void remove(int i){erase(i);}
};
struct Option {String label,detail;bool enabled=true,selected=false;uint8_t uiKind=0,uiChannel=0;int16_t uiMetric=-32768;};
uint16_t pixels[320*170];uint16_t *frame=pixels,*previousFrame=nullptr;
bool dirty=true;bool previousValid=false,retainedCanvas=false,benchmarkControl=false;uint32_t lastPresentBytes=0,pendingInputAt=0,inputQuietAt=0;uint32_t inputSamples[32];unsigned inputCount=0;
uint32_t physicalInputAt=0,consumedInputAt=0;
void prepareBackground(bool){}void flushInputMeasurements(){}
bool isScreenOff=false,isSleeping=false;int tftWidth=320,tftHeight=170;
int interpreter_state=0;
delta::Appearance appearance;delta::Status status;
uint32_t now=100, lastStatusAt=0;char currentLabel[32]="Settings";
uint32_t millis(){return now;}uint32_t micros(){return now*1000;}
void initialize(){}
void refreshStatus(uint32_t value){lastStatusAt=value;}
struct {uint16_t bgColor=0,priColor=0xf800,secColor=0x07e0;} fluxConfig;
constexpr uint32_t TFT_BLACK=0,TFT_WHITE=65535;
struct Tft {
 uint16_t displayed=0;int clears=0;Tft*native(){return this;}void fillScreen(int color){assert(color==0);displayed=0;++clears;}
 int pushes=0;bool swap=false;int lastX=0,lastY=0,lastW=0,lastH=0;const uint16_t*data=nullptr;
 bool getSwapBytes(){return swap;}void setSwapBytes(bool b){swap=b;}
 void pushImage(int x,int y,int w,int h,const uint16_t*p){displayed=*p;++pushes;lastX=x;lastY=y;lastW=w;lastH=h;data=p;}
}tft;
int renders=0;
void deltaUiPresent(int=0,int=0,int=320,int=170);
uint16_t *deltaUiCanvas(bool=true,bool=true,bool=false);
'''
        source += declarations
        source += "\nvoid renderWork(uint32_t at){++renders;workFrameAt=at;}\n"
        source += function(production, "void capturePhysicalInput(") + "\n"
        for signature in ("void deltaUiInputConsumed(", "uint16_t *deltaUiCanvas(", "void deltaUiPresent(", "bool deltaUiMenu(",
                          "void deltaUiMenuTick(", "void deltaUiMenuEnd(", "void deltaUiInvalidate(", "bool deltaUiRestore(", "uint32_t deltaUiMapColor("):
            source += function(production, signature) + "\n"
        source += r'''
int main(){
 assert(deltaUiCanvas(true,false)==pixels);
 assert(pixels[0]==delta::ui::palette(appearance.theme).background);
 workActive=true;deltaUiCanvas(false);assert(!workActive);
 deltaUiPresent(10,20,3,2);assert(tft.pushes==2&&tft.data==pixels+21*320+10&&tft.lastW==3&&tft.lastH==1&&!tft.swap);
 uint16_t shadow[320*170];previousFrame=shadow;retainedCanvas=true;previousValid=false;
 int cold=tft.pushes;deltaUiPresent();assert(tft.pushes==cold+1&&previousValid&&lastPresentBytes==108800);
 cold=tft.pushes;deltaUiPresent();assert(tft.pushes==cold&&lastPresentBytes==0);
 pixels[55*320+20]^=1;deltaUiPresent();assert(tft.pushes==cold+1&&tft.lastY==55&&tft.lastH==1&&lastPresentBytes==640);
 assert(memcmp(shadow,pixels,sizeof(pixels))==0);
 deltaUiMenuEnd();deltaUiCanvas(false);retainedCanvas=true;cold=tft.pushes;deltaUiPresent();assert(tft.pushes==cold+1&&lastPresentBytes==108800);
 isScreenOff=true;pixels[55*320+20]^=1;cold=tft.pushes;deltaUiPresent();assert(tft.pushes==cold);isScreenOff=false;
 dirty=false;lastStatusAt=123;deltaUiInvalidate();assert(!previousValid&&dirty&&lastStatusAt==0);deltaUiPresent();assert(lastPresentBytes==108800);
 retainedCanvas=false;previousFrame=nullptr;
 int before=tft.pushes;deltaUiPresent(-5,-5,7,7);assert(tft.pushes==before+2&&tft.lastX==0&&tft.lastW==2);
 before=tft.pushes;deltaUiPresent(319,169,100,100);assert(tft.pushes==before+1&&tft.lastW==1&&tft.lastH==1);
 deltaUiPresent(500,0,10,10);assert(tft.pushes==before+1);
 isScreenOff=true;assert(!deltaUiCanvas());deltaUiPresent();assert(tft.pushes==before+1);isScreenOff=false;
 std::vector<Option> options(8);for(int i=0;i<8;++i)options[i].label="A long setting";
 options[3].label="Status: On";options[4].enabled=false;
 assert(deltaUiMenu("Appearance",options,3));assert(workFirst==2&&workSelected==3&&workRows[3].label=="Status"&&workRows[3].value=="On");
 assert(workView[3].flags&delta::ui::Checked);assert(!workView[4].enabled);
 options.clear();assert(std::string(workView[3].label)=="Status"); // Callback-owned vectors may die.
 int old=renders;now+=16;deltaUiMenuTick();assert(renders==old+1);
 now+=1000;deltaUiMenuEnd();deltaUiMenuTick();assert(renders==old+1);
 options.resize(2);options[0].label="PREVIEW_AP";options[0].detail="WPA2 CH 6";options[0].uiKind=1;options[0].uiMetric=-63;options[0].uiChannel=6;options[1].label="Back";
 assert(deltaUiMenu("Networks",options,0));assert(radioView.size()==2&&radioView[0].metric==-63&&radioView[0].channel==6&&radioView[1].metric==-32768);
 isSleeping=true;old=renders;now+=16;deltaUiMenuTick();assert(renders==old);isSleeping=false;
 retainedCanvas=true;int beforeRestore=tft.pushes;assert(deltaUiRestore());assert(tft.pushes==beforeRestore+1&&lastPresentBytes==108800);
 deltaUiMenuEnd(); // Actual callback dispatch revokes DELTA ownership before a direct QR draw.
 int directPushes=tft.pushes,directClears=tft.clears;tft.displayed=0x5a5a; // Emulate native QR pixels replacing the last DELTA payload.
 assert(!deltaUiRestore()&&tft.pushes==directPushes&&tft.clears==directClears&&tft.displayed==0x5a5a);
 frame=nullptr;assert(!deltaUiRestore()&&tft.clears==directClears);frame=pixels;
 retainedCanvas=true;
 frame=nullptr;assert(!deltaUiRestore()&&tft.clears==1);frame=pixels;
 frame=nullptr;assert(!deltaUiMenu("Low memory",options,0)&&!deltaUiCanvas());frame=pixels;
 tftWidth=170;assert(!deltaUiCanvas()&&!deltaUiMenu("Rotation",options,0));tftWidth=320;
 assert(deltaUiMapColor(TFT_BLACK)==delta::ui::palette(appearance.theme).background);
 assert(deltaUiMapColor(0x00ffffff)==0x00ffffff&&deltaUiMapColor(0x1234)==0x1234);
 assert(deltaUiMapColor(fluxConfig.priColor)==delta::ui::palette(appearance.theme).accent);
 interpreter_state=2;assert(deltaUiMapColor(TFT_BLACK)==TFT_BLACK&&deltaUiMapColor(fluxConfig.priColor)==fluxConfig.priColor);
 interpreter_state=3;assert(deltaUiMapColor(TFT_WHITE)==TFT_WHITE&&deltaUiMapColor(TFT_BLACK)==TFT_BLACK);
 physicalInputAt=1000000;now=2350;pendingInputAt=0;inputCount=0;
 deltaUiMenu("New page",options,0);assert(pendingInputAt==0); // Old BLE stamp must not become a new sample.
 deltaUiInputConsumed();assert(pendingInputAt==1000000); // Only an actual drain dispatch marks input.
 bool renderedByLambda=true;const int MENU_TYPE_MAIN=0;int menuType=MENU_TYPE_MAIN;
 ACTUAL_HOME_OWNERSHIP
 assert(pendingInputAt==1000000); // Home hover must retain the dispatched event until presentation.
 deltaUiPresent();assert(inputCount==1&&inputSamples[0]==1350000); // Diagnostic reproduces arbitrary stale time only when deliberately dispatched.
 pendingInputAt=123;menuType=1;ACTUAL_HOME_OWNERSHIP
 assert(pendingInputAt==0); // A custom handler cancels incomplete navigation measurement.
}
'''
        display = (ROOT / "src/core/display.cpp").read_text(encoding="utf-8")
        ownership = "if (renderedByLambda && menuType != MENU_TYPE_MAIN) deltaUiMenuEnd();"
        self.assertIn(ownership, display)
        self.assertIn("if (rawEncoderDelta != 0) deltaUiInputConsumed();", display)
        source = source.replace("ACTUAL_HOME_OWNERSHIP", ownership)
        zig = next((ROOT / ".tools/zig").rglob("zig.exe"))
        with tempfile.TemporaryDirectory() as folder:
            cpp = Path(folder) / "runtime.cpp"
            exe = Path(folder) / "runtime.exe"
            cpp.write_text(source, encoding="utf-8")
            subprocess.run([str(zig), "c++", "-std=c++11", "-O2", "-UNDEBUG", "-I", str(ROOT / "include"), str(cpp), "-o", str(exe)], check=True)
            subprocess.run([str(exe)], check=True)


if __name__ == "__main__":
    unittest.main()
