"""Native renderer checks; no radio or device operations."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class DeltaCyberModesTests(unittest.TestCase):
    def test_native_mode_painters(self):
        zig = next((ROOT / '.tools/zig').rglob('zig.exe'))
        with tempfile.TemporaryDirectory(prefix='delta-cyber-modes-') as temporary:
            executable = Path(temporary) / 'modes.exe'
            subprocess.run([str(zig), 'c++', '-std=c++11', '-O2', '-UNDEBUG',
                            '-I', str(ROOT / 'include'), str(ROOT / 'tests/cyber_modes_test.cpp'),
                            '-o', str(executable)], check=True, cwd=ROOT)
            subprocess.run([str(executable)], check=True, cwd=ROOT)

if __name__ == '__main__':
    unittest.main()
