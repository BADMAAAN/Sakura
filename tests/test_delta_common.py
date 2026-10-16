"""Execute the firmware's wrapping, confirmation and progress entry points."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def extract(signature):
    text = (ROOT / "src/core/display.cpp").read_text(encoding="utf-8")
    start = text.index(signature)
    opening = text.index("{", start)
    end, depth = opening + 1, 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


class DeltaCommonTests(unittest.TestCase):
    def test_actual_common_edge_cases(self):
        source = r'''
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>
#include "delta/widgets.h"
struct String : std::string {
    using std::string::string;
    String(const std::string &s) : std::string(s) {}
    String substring(int start, int end=0x7fffffff) const {
        if(start>=int(size()))return String();
        return substr(start, end-start);
    }
    int indexOf(char c,int start=0)const {auto i=find(c,start);return i==npos?-1:int(i);}
};
constexpr int FM=2,TFT_BLACK=0,TFT_WHITE=65535,TC_DATUM=1,BC_DATUM=2;
int tftWidth=320,tftHeight=170,delays=0,presents=0;
bool PrevPress=false,EscPress=false,NextPress=false,SelPress=false,useCanvas=true;
bool check(bool &key){bool result=key;key=false;return result;}
void delay(int){assert(++delays<10);}
struct Tft {
    template<class...T>void setTextColor(T...){}
    template<class...T>void setTextSize(T...){}
    template<class...T>void setTextDatum(T...){}
    template<class...T>void drawString(T...){}
    template<class...T>void fillRoundRect(T...){}
    template<class...T>void drawRoundRect(T...){}
    template<class...T>void drawRect(T...){}
    void fillRect(int,int,int width,int,uint16_t){assert(width>=0&&width<=320);}
}tft;
struct Config {uint16_t bgColor=0,priColor=1;}fluxConfig;
uint16_t canvas[delta::Width*delta::Height];
delta::Appearance appearance;
delta::Status status;
uint16_t*deltaUiCanvas(bool=true,bool=true){return useCanvas?canvas:nullptr;}
const delta::Appearance&deltaUiAppearance(){return appearance;}
const delta::Status&deltaUiStatus(){status.battery=63;return status;}
void deltaUiPresent(int=0,int=0,int=320,int=170){++presents;}
void displayRedStripe(const String&,uint16_t,uint16_t){}
'''
        source += extract("std::vector<String> wrapText(")
        source += extract("void drawButton(")
        source += extract("int8_t displayMessage(")
        source += extract("void progressHandler(")
        source += r'''
int main(){
    auto lines=wrapText("ABCDEFGHIJ",4);
    assert(lines.size()==3&&lines[0]=="ABCD"&&lines[1]=="EFGH"&&lines[2]=="IJ");
    lines=wrapText("abc-def_gh",5);
    String joined;for(const auto&line:lines)joined+=line;
    assert(joined=="abc-def_gh");
    lines=wrapText("ab cd ef",5);
    assert(lines.size()==2&&lines[0]=="ab"&&lines[1]=="cd ef");
    assert(wrapText("abc",0).empty());
    assert(displayMessage("empty",nullptr,nullptr,nullptr,1)==-1);
    EscPress=true;
    assert(displayMessage("cancel","No",nullptr,"Yes",1)==-1);
    NextPress=SelPress=true;
    assert(displayMessage("confirm","No",nullptr,"Yes",1)==1);
#ifdef FLUX_DELTA_BUILD
    assert(canvas[7*delta::Width+299]==delta::ui::palette(appearance.theme).muted);
#endif
    NextPress=SelPress=true;
    assert(displayMessage("single",nullptr,"OK",nullptr,1)==0);
    useCanvas=false;EscPress=true;
    assert(displayMessage("fallback","No",nullptr,"Yes",1)==-1);
    progressHandler(0,0,"Unknown total");
    progressHandler(-1,50,"Negative");
    progressHandler(2147483647,1,"Complete");
    useCanvas=true;progressHandler(1,size_t(-1),"Large total");
#ifdef FLUX_DELTA_BUILD
    assert(presents>0);
#endif
}
'''
        compiler = next((ROOT / ".tools/zig").rglob("zig.exe"))
        with tempfile.TemporaryDirectory(prefix="delta-common-") as temp:
            cpp = Path(temp) / "common.cpp"
            cpp.write_text(source, encoding="utf-8")
            for delta in (False, True):
                exe = Path(temp) / ("delta.exe" if delta else "legacy.exe")
                flags = ["-DFLUX_DELTA_BUILD=1"] if delta else []
                result = subprocess.run([str(compiler), "c++", "-std=c++11", "-O2", "-UNDEBUG", *flags,
                                         "-I", str(ROOT / "include"), str(cpp), "-o", str(exe)],
                                        capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                result = subprocess.run([str(exe)], capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
