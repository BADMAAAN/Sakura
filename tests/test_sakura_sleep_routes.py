"""Every core deep-sleep entry must shut the screen down before sleeping."""
from pathlib import Path
import subprocess
import tempfile
import unittest
from test_delta_runtime import function

ROOT = Path(__file__).resolve().parents[1]


class SleepRouteTests(unittest.TestCase):
    def test_actual_menu_and_serial_routes(self):
        source = r'''
#include <cassert>
#include <vector>
#include <cstdint>
#define DEEPSLEEP_WAKEUP_PIN 6
#define DEEPSLEEP_PIN_ACT 0
#define SOC_PM_SUPPORT_EXT0_WAKEUP 1
using gpio_num_t = int;
struct cmd {};
std::vector<int> order;
void sleepModeOn(){order.push_back(1);}
void esp_sleep_enable_ext0_wakeup(int pin,int level){assert(pin==6&&level==0);order.push_back(2);}
void esp_deep_sleep_start(){assert(!order.empty()&&order.front()==1);order.push_back(3);}
void powerOff(){}
'''
        source += function((ROOT / 'src/core/mykeyboard.cpp').read_text(), 'void goToDeepSleep()')
        source += function((ROOT / 'src/core/serial_commands/power_commands.cpp').read_text(),
                           'uint32_t poweroffCallback(')
        source += '''int main(){goToDeepSleep();assert((order==std::vector<int>{1,2,3}));
order.clear();assert(poweroffCallback(nullptr));assert((order==std::vector<int>{1,3}));}'''
        zig = next((ROOT / '.tools/zig').rglob('zig.exe'))
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / 'routes.cpp').write_text(source)
            subprocess.run([str(zig), 'c++', '-std=c++11', '-O2', '-UNDEBUG',
                            str(path / 'routes.cpp'), '-o', str(path / 'routes.exe')], check=True)
            subprocess.run([str(path / 'routes.exe')], check=True)


if __name__ == '__main__':
    unittest.main()
