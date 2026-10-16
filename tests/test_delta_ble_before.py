"""Verify the source-extracted BLE baseline and actual TFT font-1 fixture adapter."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class BleBeforeTests(unittest.TestCase):
    def test_extracted_baseline_and_raw_sprite(self):
        source = subprocess.check_output(["git", "show", "e4ba2cd9:src/modules/ble/ble_spam.cpp"], cwd=ROOT).decode("utf-8")
        header = (ROOT / "tools/delta_preview/ble_before.h").read_text(encoding="utf-8")
        for signature in ("static void bleSpamRenderList(", "static void bleSpamRenderConfigRows(",
                          "static BleSpamListMetrics bleSpamGetListMetrics("):
            start = source.index(signature)
            body = source[start:source.index("\n}\n", start) + 3]
            body = body.replace("#if defined(FLUX_BUILD) && defined(HAS_SCREEN)",
                                "#if 1 // Device build defines FLUX_BUILD and HAS_SCREEN")
            self.assertIn(body, header)
        start = source.index("        if (redrawRows) {", source.index("static bool\nbleSpamConfigScreen("))
        self.assertIn(source[start:source.index("\n\n        if (EscPress", start)], header)
        zig = next((ROOT / ".tools/zig").rglob("zig.exe"))
        code = r'''
#include "tools/delta_preview/ble_before.h"
#include <cassert>
int main(){
 uint16_t guarded[320*170+2];guarded[0]=0x1234;guarded[320*170+1]=0xabcd;auto*p=guarded+1;
 deltaBleBefore(p,0);assert(p[55*320+305]==0); // actual raw black sprite viewport
 assert(p[60*320+305]==0xa80f); // actual unmapped selected row
 assert(guarded[0]==0x1234&&guarded[320*170+1]==0xabcd);
 deltaBleBefore(p,1);assert(p[55*320+305]==delta::ui::palette(delta::Theme::DarkSakura).background); // config uses the mapped background; NIGHT black quantizes to zero
 assert(guarded[0]==0x1234&&guarded[320*170+1]==0xabcd);
 using namespace delta_ble_before;
 Raster glyph(false);glyph.p=p;glyph.setTextColor(0xffff,0);glyph.drawString("A",0,0,1);
 for(int y=0;y<8;++y)for(int x=0;x<6;++x)assert(p[y*320+x]==((x<5&&(font['A'*5+x]&(1<<y)))?0xffff:0));
}
'''
        with tempfile.TemporaryDirectory(prefix="delta-ble-before-") as temporary:
            exe = Path(temporary) / "before.exe"
            subprocess.run([str(zig), "c++", "-std=c++11", "-O2", "-UNDEBUG", "-I", str(ROOT),
                            "-I", str(ROOT / "include"), "-x", "c++", "-", "-o", str(exe)],
                           input=code, text=True, check=True, cwd=ROOT)
            subprocess.run([str(exe)], check=True, cwd=ROOT)


if __name__ == "__main__":
    unittest.main()
