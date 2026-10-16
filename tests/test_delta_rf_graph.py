"""Exercise the shared graph and real CC1101 presentation path with inert RF stubs."""
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class DeltaRfGraphTests(unittest.TestCase):
    def compile_run(self, source):
        compiler = next((ROOT / ".tools/zig").rglob("zig.exe"))
        with tempfile.TemporaryDirectory(prefix="delta-rf-graph-") as temp:
            cpp, exe = Path(temp) / "rf.cpp", Path(temp) / "rf.exe"
            cpp.write_text(source, encoding="utf-8")
            result = subprocess.run([str(compiler), "c++", "-std=c++11", "-O2", "-UNDEBUG",
                                     "-I", str(ROOT / "include"), str(cpp), "-o", str(exe)],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_shared_graph(self):
        self.compile_run((ROOT / "tests/delta_rf_graph_test.cpp").read_text(encoding="utf-8"))

    def test_actual_acquisition_order_and_display(self):
        production = (ROOT / "src/modules/rf/rf_spectrum.cpp").read_text(encoding="utf-8")
        start = production.index("void rf_CC1101_rssi()")
        # This is the last function in the production file.
        source = r'''
#include "delta/rf_graph.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <string>
#include <vector>
#define FLUX_DELTA_BUILD 1
#define pdMS_TO_TICKS(x) (x)
using String=std::string;
constexpr int CC1101_SPI_MODULE=1;
int tftWidth=320,tftHeight=170,reads=0,target=1,sentinels=0,presents=0,delay5=0,delay75=0,inits=0,deinits=0;
bool EscPress=false,SelPress=false,canvasReady=true;
struct Pins{int rfModule=1;bool rfFxdFreq=true;float rfFreq=433.92f;int rfScanRange=0;}fluxConfigPins;
float subghz_frequency_list[]={433.05f,433.20f,434.79f};
const char*subghz_frequency_ranges[]={"433-435"};
int range_limits[][2]={{0,2}};
std::vector<float> tuned;
struct Radio{int getRssi(){return -61+reads++;}}ELECHOUSE_cc1101;
bool initRfModule(const char*,float){++inits;return true;}
void deinitRfModule(){++deinits;}
void setMHZ(float frequency){tuned.push_back(frequency);}
void rf_range_selection(float) {assert(false);}
void displayError(const char*,bool=false){assert(false);}
bool check(bool&key){return &key==&EscPress && reads>=target;}
void vTaskDelay(int ticks){if(ticks==5)++delay5;else if(ticks==75)++delay75;else assert(false);}
int map(int value,int low,int high,int outLow,int outHigh){return (value-low)*(outHigh-outLow)/(high-low)+outLow;}
struct Tft{
 void drawPixel(int x,int y,int color){assert(x==0&&y==0&&color==0);++sentinels;}
 template<class...T>void fillScreen(T...){}template<class...T>void setTextSize(T...){}
 template<class...T>void setTextColor(T...){}template<class...T>void setCursor(T...){}
 template<class...T>void printf(T...){}template<class...T>void drawString(T...){}
 template<class...T>void drawRightString(T...){}template<class...T>void drawCentreString(T...){}
 template<class...T>void drawFastVLine(T...){}template<class...T>void drawFastHLine(T...){}
 template<class...T>void drawLine(T...){}template<class...T>void fillRect(T...){}
}tft;
uint16_t pixels[delta::Width*delta::Height];
delta::Appearance appearance;
uint16_t*deltaUiCanvas(bool=true,bool=true){return canvasReady?pixels:nullptr;}
const delta::Appearance&deltaUiAppearance(){return appearance;}
void deltaUiPresent(){++presents;}
uint16_t spectrumBg(){return 0;}uint16_t spectrumAccent(){return 1;}
'''
        source += production[start:]
        source += r'''
void reset(){reads=sentinels=presents=delay5=delay75=inits=deinits=0;tuned.clear();}
int main(){
 using namespace delta::ui;
 rf_CC1101_rssi();
 assert(reads==1&&sentinels==2&&delay75==1&&delay5==0&&tuned.empty()&&inits==1&&deinits==1&&presents==2);
 std::vector<int16_t> history(300,RssiUnmeasured);history.back()=-61;
 std::vector<uint16_t> expected(delta::Width*delta::Height);
 rfGraph(expected.data(),appearance,history.data(),nullptr,300,433.92f);
 assert(std::equal(expected.begin(),expected.end(),pixels));
 reset();target=3;fluxConfigPins.rfFxdFreq=false;rf_CC1101_rssi();
 assert(reads==3&&sentinels==4&&delay5==3&&delay75==0&&tuned.size()==3&&inits==1&&deinits==1&&presents==2);
 for(int i=0;i<3;++i)assert(tuned[i]==subghz_frequency_list[i]);
 const int16_t bins[]={-61,-60,-59};
 rfGraph(expected.data(),appearance,bins,subghz_frequency_list,3);
 assert(std::equal(expected.begin(),expected.end(),pixels));
}
'''
        self.compile_run(source)
        # The single-symbol map keeps a nonzero denominator.
        self.assertRegex(production, r"map\(i, 0, rx_size > 1 \? rx_size - 1 : 1,")


if __name__ == "__main__":
    unittest.main()
