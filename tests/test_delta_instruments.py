"""Check native instrument views without initiating radio activity."""
from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]

class DeltaInstrumentsTests(unittest.TestCase):
    def test_compiled_real_data_instruments(self):
        zig = next((ROOT / ".tools/zig").rglob("zig.exe"), None)
        self.assertIsNotNone(zig)
        exe = ROOT / "tools/delta_preview/delta_instruments_test.exe"
        subprocess.run([str(zig), "c++", "-std=c++11", "-O2", "-UNDEBUG", str(ROOT / "tests/delta_instruments_test.cpp"), "-o", str(exe)], check=True, cwd=ROOT)
        subprocess.run([str(exe)], check=True, cwd=ROOT)

if __name__ == "__main__":
    unittest.main()
