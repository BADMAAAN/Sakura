"""The BLE skin paints real firmware pixels without executing a radio handler."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SOURCE = "src/modules/ble/ble_spam.cpp"


def function(source, signature):
    start = source.index(signature)
    end = source.index("\n}\n", start) + 3
    return source[start:end]


class DeltaBleSurfaceTests(unittest.TestCase):
    def test_firmware_painter_and_continuous_background(self):
        zig = next((ROOT / ".tools/zig").rglob("zig.exe"))
        code = r'''
#include "delta/ble_surface.h"
#include <cassert>
#include <cstring>
#include <string>
#include <algorithm>
static uint16_t guarded[delta::Width*delta::Height+2];
int main() {
    using namespace delta; using namespace delta::ui;
    auto*p=guarded+1; guarded[0]=0x1234; guarded[Width*Height+1]=0xabcd;
    const char*labels[]={"Apple Pairing","Apple Action","Android FastPair","Windows Swift Pair"};
    const char*values[]={"20 ms","100 ms","+21 dBm","Every packet"};
    Appearance a; Status status;status.time="21:04";status.battery=76;
    for(int theme=0;theme<3;++theme) {
        a.theme=Theme(theme);
        for(int view=0;view<5;++view) {
            memset(p,0,Width*Height*sizeof(*p));
            if(view==0)bleList(p,a,"BLE Spam",labels,4,0,8,2,&status);
            if(view==1)bleConfig(p,a,"Android FastPair",values,4,-1,&status);
            if(view==2)bleConfig(p,a,"Android FastPair",values,0,0,&status);
            if(view==3)bleRunning(p,a,"Android FastPair",values,1,-1,17,3.5f,true,&status);
            if(view==4)blePrompt(p,a,"Saved name","Use Saved Name","Delete Saved Name",1);
            assert(guarded[0]==0x1234&&guarded[Width*Height+1]==0xabcd);
            // The former raw TFT sprite made this complete viewport solid zero.
            uint16_t background[Width*Height];workBackground(background,a);
            for(int y=62;y<150;++y) {assert(p[y*320]==background[y*320]);assert(p[y*320+319]==background[y*320+319]);}
            int contentPixels=0;for(int y=62;y<150;++y)for(int x=14;x<300;++x)contentPixels+=p[y*320+x]!=background[y*320+x];
            assert(contentPixels>80); // Real content over continuous background, not a blank replacement.
        }
    }
    bleList(nullptr,a,"",nullptr,0,0,0,0);
    bleConfig(nullptr,a,"",values,0,-1);
    bleList(p,a,"Empty",nullptr,0,0,0,0);
    {
        using String=std::string;using std::min;
        struct {int scroll;}state{0}; int itemCount=4;
        auto labelFn=[](int i)->const char*{static char name[16];snprintf(name,sizeof(name),"saved-%d",i);return name;};
        /*ACTUAL_LABEL_PROJECTION*/
        assert(std::string(labels[0])=="saved-0"&&std::string(labels[3])=="saved-3");
    }
}
'''
        firmware = (ROOT / SOURCE).read_text(encoding="utf-8")
        start = firmware.index("        String retainedLabels[4];")
        end = firmware.index("        delta::ui::bleList(", start)
        code = code.replace("/*ACTUAL_LABEL_PROJECTION*/", firmware[start:end])
        with tempfile.TemporaryDirectory(prefix="delta-ble-pixels-") as temporary:
            exe = Path(temporary) / "surface.exe"
            subprocess.run([str(zig), "c++", "-std=c++11", "-O2", "-UNDEBUG",
                            "-I", str(ROOT / "include"), "-x", "c++", "-", "-o", str(exe)],
                           input=code, text=True, check=True, cwd=ROOT)
            subprocess.run([str(exe)], check=True, cwd=ROOT)

    def test_radio_payload_and_stats_are_unchanged(self):
        before = subprocess.check_output(["git", "show", "e4ba2cd9:" + SOURCE], cwd=ROOT).decode("utf-8")
        after = (ROOT / SOURCE).read_text(encoding="utf-8")
        for signature in ("static void\nbleSpamSendTick(",
                          "static void bleSpamUpdateStats(", "static void bleSpamMenuUi(",
                          "static bool bleSpamHandleCustomNameDevice("):
            self.assertEqual(function(before, signature), function(after, signature), signature)
        # Back routing is intentionally corrected by the hotfix; executable
        # regression scenarios live in test_sakura_back.py.


if __name__ == "__main__":
    unittest.main()
