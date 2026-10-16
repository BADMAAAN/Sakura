"""Compile the real boot dispatcher gate with no radio, display or storage side effects."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class StartupConfirmationTests(unittest.TestCase):
    def test_delta_confirmation_and_legacy_autostart(self):
        text = (ROOT / 'src/main.cpp').read_text(encoding='utf-8')
        begin = text.index('void begin_startup_app()')
        opening = text.index('{', begin)
        end, depth = opening + 1, 1
        while depth:
            depth += (text[end] == '{') - (text[end] == '}')
            end += 1
        helper = text[begin:end]
        source = r'''
#include <assert.h>
#include <string>
#include <vector>
#include <functional>
using String = std::string;
#define MENU_TYPE_SUBMENU 1
struct Option { String label; std::function<void()> operation; };
int prompts=0, dispatches=0, writes=0, choice=0, highlighted=0;
bool valid=true;
String launched;
struct Config {
    String startupApp;
    void setStartupApp(const String &value) { ++writes; startupApp=value; }
} fluxConfig;
struct Startup {
    bool startApp(const String &app) { ++dispatches; launched=app; return valid; }
} startupApp;
int loopOptions(std::vector<Option> &options, int type, const char *, int selected) {
    ++prompts;
    assert(type==MENU_TYPE_SUBMENU && selected==0 && options.size()==2);
    assert(options[0].label=="Main Menu");
    assert(options[1].label=="Run: Custom SubGHz");
    if (choice >= 0) options[choice].operation();
    return highlighted;
}
'''
        source += helper
        source += r'''
int main() {
    begin_startup_app();
    assert(prompts==0 && dispatches==0 && writes==0);
    fluxConfig.startupApp="Custom SubGHz";
#ifdef FLUX_DELTA_BUILD
    begin_startup_app(); // Default Main Menu never dispatches or changes stored selection.
    assert(prompts==1 && dispatches==0 && writes==0 && fluxConfig.startupApp=="Custom SubGHz");
    choice=-1;
    begin_startup_app(); // Back/cancel is equally safe.
    assert(prompts==2 && dispatches==0 && writes==0 && fluxConfig.startupApp=="Custom SubGHz");
    highlighted=1;
    begin_startup_app(); // A highlighted Run is not confirmation when the menu exits.
    assert(dispatches==0 && writes==0);
    choice=0;
    begin_startup_app(); // Remote Main Menu may differ from the highlighted index.
    assert(dispatches==0 && writes==0);
    highlighted=0;
    choice=1;
    begin_startup_app();
    assert(prompts==5 && dispatches==1 && writes==0 && launched=="Custom SubGHz");
    valid=false;
    begin_startup_app(); // Invalid saved entry is preserved, never silently cleared.
    assert(dispatches==2 && writes==0 && fluxConfig.startupApp=="Custom SubGHz");
#else
    begin_startup_app();
    assert(prompts==0 && dispatches==1 && writes==0 && launched=="Custom SubGHz");
    valid=false;
    begin_startup_app();
    assert(prompts==0 && dispatches==2 && writes==1 && fluxConfig.startupApp.empty());
#endif
}
'''
        compiler = next((ROOT / '.tools/zig').rglob('zig.exe'))
        with tempfile.TemporaryDirectory(prefix='delta-startup-') as temp:
            cpp = Path(temp) / 'startup.cpp'
            cpp.write_text(source, encoding='utf-8')
            for delta in (False, True):
                exe = Path(temp) / ('delta.exe' if delta else 'legacy.exe')
                flags = ['-DFLUX_DELTA_BUILD=1'] if delta else []
                result = subprocess.run([str(compiler), 'c++', '-std=c++11', '-O2', '-UNDEBUG', *flags, str(cpp), '-o', str(exe)], capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                result = subprocess.run([str(exe)], capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

if __name__ == '__main__':
    unittest.main()
