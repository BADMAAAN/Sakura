"""Run native renderer checks against the exact packed glyphs linked by firmware."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class DeltaTypographyTests(unittest.TestCase):
    def test_native_typography(self):
        zig = next((ROOT / ".tools/zig").rglob("zig.exe"), None)
        self.assertIsNotNone(zig, "Existing portable C++ compiler is required")
        with tempfile.TemporaryDirectory(prefix="delta-typography-") as output:
            executable = Path(output) / "typography-test.exe"
            subprocess.run([str(zig), "c++", "-std=c++11", "-O2", "-UNDEBUG", str(ROOT / "tests/delta_typography_test.cpp"), "-o", str(executable)], check=True, cwd=ROOT)
            subprocess.run([str(executable)], check=True, cwd=ROOT)
            # Exercise the actual firmware extern/one-definition arrangement too.
            subprocess.run([str(zig), "c++", "-std=c++11", "-O2", "-UNDEBUG", "-DARDUINO", "-DFLUX_DELTA_BUILD", "-I", str(ROOT / "include"), str(ROOT / "tests/delta_typography_test.cpp"), str(ROOT / "src/core/delta_typography.cpp"), "-o", str(executable)], check=True, cwd=ROOT)
            subprocess.run([str(executable)], check=True, cwd=ROOT)


if __name__ == "__main__":
    unittest.main()
