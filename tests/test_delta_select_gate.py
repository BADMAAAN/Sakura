"""Compile the actual encoder-menu release gate: held entry cannot auto-select."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class SelectGateTests(unittest.TestCase):
    def test_actual_release_gate(self):
        source = (ROOT / 'src/core/display.cpp').read_text(encoding='utf-8')
        gate = source.split('// RELEASE GATE:', 1)[1].split('// END RELEASE GATE', 1)[0]
        gate = gate[gate.index('\n'):]
        code = '''#include <cassert>
bool SelPress=false;int pin=0;const int SEL_BTN=1,BTN_ACT=0;
bool check(bool&v){bool old=v;v=false;return old;}int digitalRead(int){return pin;}
int main(){bool selectReleased=false,selectAllowed=false;
auto poll=[&](){GATE return selectAllowed&&check(SelPress);};
for(int i=0;i<1000;++i){SelPress=true;assert(!poll());} // Held entry never executes.
pin=1;assert(!poll()); // Release consumes the old press.
pin=0;SelPress=true;assert(poll());assert(!poll()); // Fresh press immediately, once.
}'''.replace('GATE', gate)
        zig = next((ROOT / '.tools/zig').rglob('zig.exe'))
        with tempfile.TemporaryDirectory() as directory:
            exe = Path(directory) / 'gate.exe'
            subprocess.run([str(zig), 'c++', '-std=c++11', '-O2', '-UNDEBUG', '-x', 'c++', '-', '-o', str(exe)], input=code, text=True, check=True)
            subprocess.run([str(exe)], check=True)
