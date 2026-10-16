from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[1]
class SakuraRendererTests(unittest.TestCase):
    def test_native_renderer(self):
        zig = next((ROOT / '.tools/zig').rglob('zig.exe'))
        with tempfile.TemporaryDirectory(prefix='sakura-render-') as folder:
            exe = Path(folder) / 'renderer.exe'
            subprocess.run([str(zig), 'c++', '-std=c++11', '-O2', '-UNDEBUG', '-I', str(ROOT / 'include'), str(ROOT / 'tests/sakura_renderer_test.cpp'), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)
    def test_actual_wake_presents_before_backlight(self):
        from test_delta_runtime import function
        production = (ROOT / 'src/core/display.cpp').read_text(encoding='utf-8')
        code = r"""
#include <cassert>
#include <vector>
#include <cstdint>
std::vector<int> order;
bool isSleeping=false,isScreenOff=true,dimmer=true;
uint32_t previousMillis=0;
uint32_t millis(){return 123;}
void deltaUiInvalidate(){order.push_back(1);}
void panelSleep(bool on){assert(!on);order.push_back(2);}
bool deltaUiRestore(){assert(!isScreenOff);order.push_back(3);return true;}
void getBrightness(){order.push_back(4);}
int pdMS_TO_TICKS(int ms){return ms;}
void vTaskDelay(int ms){assert(ms==200);order.push_back(5);}
"""
        code += function(production, 'bool wakeUpScreen()')
        code += r"""
int main(){
 assert(wakeUpScreen());assert(previousMillis==123&&!isScreenOff&&!dimmer);
 assert((order==std::vector<int>{1,2,3,4,5}));
 order.clear();isSleeping=true;isScreenOff=true;assert(!wakeUpScreen()&&order.empty());
}
"""
        zig = next((ROOT / '.tools/zig').rglob('zig.exe'))
        with tempfile.TemporaryDirectory(prefix='sakura-wake-') as folder:
            cpp=Path(folder)/'wake.cpp';exe=Path(folder)/'wake.exe';cpp.write_text(code)
            subprocess.run([str(zig),'c++','-std=c++11','-O2','-UNDEBUG','-DFLUX_DELTA_BUILD',str(cpp),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)
if __name__ == '__main__': unittest.main()
