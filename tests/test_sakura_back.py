"""Run extracted firmware Back handlers with inert display and radio doubles."""
from pathlib import Path
import subprocess
import tempfile
import unittest
import re

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


class SakuraBackTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        spam = (ROOT / 'src/modules/ble/ble_spam.cpp').read_text(encoding='utf-8')
        common = (ROOT / 'src/modules/ble/ble_common.cpp').read_text(encoding='utf-8')
        source = (ROOT / 'tests/sakura_back_test.cpp').read_text(encoding='utf-8')
        handlers = '\n'.join(function(spam, signature) for signature in (
            'static int bleSpamListLoop(', 'static bool\nbleSpamConfigScreen(',
            'static bool bleSpamStoppedPrompt(', 'static int bleSpamTwoOptionPrompt('))
        handlers += '\n' + function(common, 'void ble_info(')
        handlers += '\n' + function(common, 'bool bleScanCancellable(')
        suite = (ROOT / 'src/modules/ble/BLE_Suite.cpp').read_text(encoding='utf-8')
        handlers += '\n' + function(suite, 'void runFastPairScan(')
        board = (ROOT / 'boards/lilygo-t-embed-cc1101/interface.cpp').read_text(encoding='utf-8')
        handlers += '\n' + function(board, 'void InputHandler(void)')
        globals_source = (ROOT / 'include/globals.h').read_text(encoding='utf-8')
        source = source.replace('/* PRODUCTION_CHECK */', function(globals_source, 'extern inline bool check('))
        source = source.replace('/* PRODUCTION_CLEAR */', function(globals_source, 'extern inline void clearInputState()'))
        source = source.replace('/* PRODUCTION_HANDLERS */', handlers)
        cls.directory = tempfile.TemporaryDirectory(prefix='sakura-back-')
        cls.exe = Path(cls.directory.name) / 'back.exe'
        zig = next((ROOT / '.tools/zig').rglob('zig.exe'))
        result = subprocess.run([str(zig), 'c++', '-std=c++11', '-O2', '-UNDEBUG',
                                 '-I', str(ROOT / 'include'), '-I', str(ROOT / 'src'), '-x', 'c++', '-', '-o', str(cls.exe)],
                                input=source, text=True, capture_output=True, cwd=ROOT)
        if result.returncode:
            cls.directory.cleanup()
            raise AssertionError(result.stdout + result.stderr)

    @classmethod
    def tearDownClass(cls):
        cls.directory.cleanup()

    def run_case(self, scenario):
        result = subprocess.run([str(self.exe), scenario], text=True, capture_output=True, timeout=5)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_back_before_start_and_results(self):
        self.run_case('list')

    def test_back_has_priority_over_rotation_and_select(self):
        self.run_case('pending')

    def test_config_back_never_starts(self):
        self.run_case('config')

    def test_back_closes_prompts_without_restart(self):
        self.run_case('prompts')

    def test_back_closes_device_wait_without_select(self):
        self.run_case('info')

    def test_scan_back_before_start(self):
        self.run_case('scan-pre')

    def test_scan_cancel_and_completion_race(self):
        self.run_case('scan-cancel')

    def test_scan_error_preserves_active_resources(self):
        self.run_case('scan-timeout')

    def test_fastpair_cancel_skips_results_and_deinit(self):
        self.run_case('fastpair')

    def test_scan_rejects_someone_elses_operation(self):
        self.run_case('scan-owned')

    def test_repeated_scan_close_has_no_running_resources(self):
        self.run_case('scan-repeat')

    def test_held_release_bounce_and_fast_select_back(self):
        self.run_case('edges')

    def test_actual_board_held_back_and_repeated_press(self):
        self.run_case('input-held')

    def test_actual_board_fast_select_back(self):
        self.run_case('input-fast')

    def test_actual_board_wake_press_cannot_select(self):
        self.run_case('input-wake')

    def test_actual_board_screen_off_wake_cannot_select(self):
        self.run_case('input-screen')

    def test_diagnostics_is_active_only_and_back_closes(self):
        production = (ROOT / 'src/core/input_diagnostics.cpp').read_text(encoding='utf-8')
        production = re.sub(r'^#include.*$', '', production, flags=re.M)
        code = r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include <functional>
#include "core/input_diagnostics.h"
#define SAKURA_BUILD 1
#define pdMS_TO_TICKS(x) (x)
#define MENU_TYPE_SUBMENU 1
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(x) ((void)0)
#define portEXIT_CRITICAL(x) ((void)0)
using portMUX_TYPE=int;
bool EscPress=false,SelPress=false,PrevPress=false,NextPress=false;
int waits=0;bool check(bool&v){bool old=v;v=false;return old;}
uint32_t micros(){return 42;}
void vTaskDelay(int){assert(++waits<3);EscPress=true;}
void drainRotarySteps(){}
namespace delta{namespace ui{
struct MenuRow{const char*label,*value;};
template<class...T>void list(T...){}
}}
uint16_t*deltaUiCanvas(){return nullptr;}
int deltaUiAppearance(){return 0;}
int status;const int&deltaUiStatus(){return status;}
void deltaUiPresent(){}
struct Option{const char*label;std::function<void()>action;};
int diagnosticMenus=0;
int loopOptions(std::vector<Option>&choices,int,const char*){
 if(++diagnosticMenus==1){EscPress=true;choices[0].action();return 0;}
 return -1;
}
'''
        code += production
        code += r'''
int main(){
 inputDiagnosticEvent(InputSource::Select,InputKind::Press,20000);assert(last.count==0);
 active=true;inputDiagnosticEvent(InputSource::Encoder,InputKind::Left,0);assert(last.count==1);
 inputDiagnosticConsumed(true);assert(!strcmp(last.action,"back/cancel"));
 active=false;inputDiagnosticEvent(InputSource::Back,InputKind::Long,700000);assert(last.count==1);
 EscPress=true;buttonsScreen();assert(!active&&!strcmp(last.screen,"menu")&&waits==0);
 buttonsScreen();assert(!active&&!strcmp(last.screen,"menu")&&waits==1);
 inputDiagnosticsMenu();assert(diagnosticMenus==2); // Buttons Back returns to Diagnostics first.
}
'''
        zig = next((ROOT / '.tools/zig').rglob('zig.exe'))
        with tempfile.TemporaryDirectory(prefix='sakura-diagnostics-') as directory:
            exe = Path(directory) / 'diagnostics.exe'
            result = subprocess.run([str(zig), 'c++', '-std=c++11', '-O2', '-UNDEBUG',
                                     '-I', str(ROOT / 'src'), '-x', 'c++', '-', '-o', str(exe)],
                                    input=code, text=True, capture_output=True, cwd=ROOT)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(exe)], text=True, capture_output=True, timeout=5)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == '__main__':
    unittest.main()
