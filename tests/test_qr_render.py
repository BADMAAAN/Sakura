"""Render the actual bundled QR encoder through a host TFT, then verify pixels."""
import ctypes
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class QrRenderTests(unittest.TestCase):
    def test_production_qr_quiet_zone_and_decode(self):
        zig = next((ROOT / '.tools/zig').rglob('zig.exe'))
        library = ROOT / 'lib/TFT_eSPI_QRcode/src'
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / 'display').mkdir()
            (path / 'Arduino.h').write_text('''#pragma once
#include <cstring>
#include <string>
#include <cstdint>
using byte = uint8_t;
struct String : std::string {
 using std::string::string;
 void toCharArray(char *p, size_t n) { std::strncpy(p, c_str(), n-1); p[n-1]=0; }
};''')
            (path / 'display/tft.h').write_text('''#pragma once
#include <cstdint>
#include <algorithm>
#define TFT_BLACK 0
#define TFT_WHITE 65535
struct tft_display {
 uint16_t pixels[320*170];
 int width(){return 320;} int height(){return 170;}
 void fillScreen(uint16_t c){std::fill(pixels,pixels+320*170,c);}
 void drawPixel(int x,int y,uint16_t c){if(x>=0&&x<320&&y>=0&&y<170)pixels[y*320+x]=c;}
 void fillRect(int x,int y,int w,int h,uint16_t c){for(int j=0;j<h;++j)for(int i=0;i<w;++i)drawPixel(x+i,y+j,c);}
};''')
            (path / 'host.cpp').write_text('''#include "Arduino.h"
#include <cstdlib>
bool allocationFails=false;
void *testAllocate(size_t n){return allocationFails?nullptr:std::malloc(n);}
#define malloc testAllocate
#include "qrcode.cpp"
#undef malloc
extern "C" __declspec(dllexport) void fail_alloc(bool fail){allocationFails=fail;}
extern "C" __declspec(dllexport) bool raw_encode(const char *s){
 std::strcpy((char *)strinbuf,s); qrframe=(unsigned char *)std::malloc(600);
 bool ok=qrencode()!=0; std::free(qrframe); qrframe=nullptr; return ok;
}
extern "C" __declspec(dllexport) bool render(const char *s, unsigned char *out){
 tft_display tft; tft.fillScreen(TFT_WHITE); QRcode qr(&tft); qr.init(); bool ok=qr.create(String(s));
 for(int i=0;i<320*170;++i)out[i]=tft.pixels[i]==TFT_BLACK?0:255;
 return ok;
}''')
            objects = []
            for name in ('qrencode', 'frame'):
                obj = path / (name + '.o')
                subprocess.run([str(zig), 'cc', '-O2', '-c', str(library / (name + '.c')),
                                '-o', str(obj)], check=True)
                objects.append(str(obj))
            subprocess.run([str(zig), 'c++', '-std=c++11', '-O2', '-shared', '-I', str(path),
                            '-I', str(library), str(path / 'host.cpp'),
                            *objects, '-o', str(path / 'qr.dll')], check=True)
            dll = ctypes.CDLL(str(path / 'qr.dll'))
            dll.render.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_uint8)]
            dll.render.restype = ctypes.c_bool
            dll.fail_alloc.argtypes = [ctypes.c_bool]
            dll.raw_encode.argtypes = [ctypes.c_char_p]
            dll.raw_encode.restype = ctypes.c_bool
            try:
                from PIL import Image
                sys.path.insert(0, str(ROOT / '.tools/qr-check'))
                import zxingcpp  # Decode is required evidence, never an optional PASS.
                for address in (b'1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa',
                                b'3J98t1WpEZ73CNmQviecrnyiWrnqRhWNLy',
                                'https://example.invalid/QR?text=Привет🌸'.encode(),
                                b'x' * 154, ('я' * 77).encode()):
                    pixels = (ctypes.c_uint8 * (320 * 170))()
                    self.assertTrue(dll.render(address, pixels))
                    image = Image.frombytes('L', (320, 170), bytes(pixels))
                    # WD=45 at 3 px/module: at least 12 white pixels all around.
                    for box in ((0, 0, 320, 12), (0, 158, 320, 170),
                                (0, 0, 12, 170), (308, 0, 320, 170),
                                (80, 5, 239, 17), (80, 152, 239, 164),
                                (80, 17, 92, 152), (227, 17, 239, 152)):
                        self.assertEqual(image.crop(box).getextrema(), (255, 255))
                    self.assertIn(0, pixels)
                    result = zxingcpp.read_barcode(image)
                    self.assertIsNotNone(result)
                    self.assertEqual(result.bytes, address)
                for address in (b'x' * 155, ('я' * 77 + 'a').encode()):
                    pixels = (ctypes.c_uint8 * (320 * 170))()
                    self.assertFalse(dll.render(address, pixels))
                    self.assertEqual(set(pixels), {255})
                    self.assertFalse(dll.raw_encode(address))
                dll.fail_alloc(True)
                self.assertFalse(dll.render(b'memory failure', pixels))
                self.assertEqual(set(pixels), {255})
            finally:
                ctypes.windll.kernel32.FreeLibrary(ctypes.c_void_p(dll._handle))


if __name__ == '__main__':
    unittest.main()
