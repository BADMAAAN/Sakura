"""Compile the same allocation-free widgets used by firmware and preview."""
from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]

class DeltaWidgetsTests(unittest.TestCase):
    def test_compiled_shared_widgets(self):
        zig = next((ROOT / ".tools/zig").rglob("zig.exe"), None)
        self.assertIsNotNone(zig, "Portable Zig is required for the shared widget check")
        exe = ROOT / "tools/delta_preview/delta_widgets_test.exe"
        subprocess.run([str(zig), "c++", "-std=c++11", "-O2", "-UNDEBUG", str(ROOT / "tests/delta_widgets_test.cpp"), "-o", str(exe)], check=True, cwd=ROOT)
        subprocess.run([str(exe)], check=True, cwd=ROOT)

if __name__ == "__main__":
    unittest.main()
