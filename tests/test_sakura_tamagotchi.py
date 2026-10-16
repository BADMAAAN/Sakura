"""Run the production updateUi renderer with explicit synthetic data, never radio code."""
from pathlib import Path
import subprocess
import tempfile
import unittest
from test_delta_runtime import function

ROOT = Path(__file__).resolve().parents[1]


class TamagotchiTests(unittest.TestCase):
    def test_actual_ui_fixture(self):
        production = (ROOT / 'src/modules/pwnagotchi/ui.cpp').read_text(encoding='utf-8')
        source = r'''
#include <delta/tamagotchi_ui.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
using String = std::string;
delta::Appearance appearance;
std::vector<uint16_t> pixels(delta::Width * delta::Height + 2, 0xabcd);
int presents=0, legacy=0, tftHeight=170;
unsigned ch=6, num_HS=0;
uint16_t *deltaUiCanvas(bool,bool,bool){return pixels.data()+1;}
const delta::Appearance &deltaUiAppearance(){return appearance;}
void deltaUiPresent(){++presents;}
uint8_t getCurrentMoodId(){return 3;}
String getCurrentMoodFace(){return "( O_O)";}
String getCurrentMoodPhrase(){return "WANTED: FRIENDS";}
bool isCurrentMoodBroken(){return false;}
String getRssiBars(int rssi){return rssi==-1000?"":"[####]";}
unsigned runPeers=0,totalPeers=0;
unsigned getPwngridRunTotalPeers(){return runPeers;}
unsigned getPwngridTotalPeers(){return totalPeers;}
String getPwngridLastFriendName(){return "FIXTURE-PEER";}
int getPwngridClosestRssi(){return runPeers?-60:-1000;}
int getBattery(){return 63;}
unsigned long millis(){return 62000;}
void drawTopCanvas(){++legacy;}
void drawTime(){++legacy;}
void drawFooterData(unsigned,unsigned,const String&,int){++legacy;}
void drawMood(const String&,const String&,bool){++legacy;}
'''
        source += function(production, 'void updateUi(bool show_toolbars)')
        source += r'''
void save(const char *name){
 FILE *f=fopen(name,"wb");assert(f);fprintf(f,"P6\n320 170\n255\n");
 for(int i=1;i<=320*170;++i){uint16_t c=pixels[i];
  unsigned char rgb[]={static_cast<unsigned char>(((c>>11)&31)*255/31),
   static_cast<unsigned char>(((c>>5)&63)*255/63),static_cast<unsigned char>((c&31)*255/31)};
  fwrite(rgb,1,3,f);
 } fclose(f);
}
int main(int argc,char**argv){
 assert(argc==2);
 updateUi(true);assert(presents==1&&legacy==0);
 assert(pixels.front()==0xabcd&&pixels.back()==0xabcd);
 const auto empty=pixels;
 delta::typeface::clip(pixels.data()+1,"TEST FIXTURE",216,7,94,
   delta::ui::palette(appearance.theme).muted,delta::typeface::Small);
 save(argv[1]);
 runPeers=2;totalPeers=5;updateUi(true);
 assert(memcmp(empty.data()+1+134*320,pixels.data()+1+134*320,320*36*2)!=0);
 assert(pixels.front()==0xabcd&&pixels.back()==0xabcd);
 for(int i=0;i<3;++i){appearance.theme=delta::Theme(i);updateUi(false);
  assert(pixels.front()==0xabcd&&pixels.back()==0xabcd);}
 assert(presents==5&&legacy==0);
}
'''
        zig = next((ROOT / '.tools/zig').rglob('zig.exe'))
        artifact = ROOT / 'artifacts/sakura-tamagotchi-test-fixture.ppm'
        artifact.parent.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='sakura-tamagotchi-') as folder:
            cpp = Path(folder) / 'fixture.cpp'
            exe = Path(folder) / 'fixture.exe'
            cpp.write_text(source, encoding='utf-8')
            subprocess.run([str(zig), 'c++', '-std=c++11', '-O2', '-UNDEBUG',
                            '-DFLUX_DELTA_BUILD', '-I', str(ROOT / 'include'),
                            str(cpp), '-o', str(exe)], check=True)
            subprocess.run([str(exe), str(artifact)], check=True)
        self.assertEqual(artifact.read_bytes()[:15], b'P6\n320 170\n255\n')
        menu = (ROOT / 'src/core/menu_items/WifiMenu.cpp').read_text(encoding='utf-8')
        self.assertIn('{"Sakura Tamagotchi", fluxgotchi_start}', menu)


if __name__ == '__main__':
    unittest.main()
