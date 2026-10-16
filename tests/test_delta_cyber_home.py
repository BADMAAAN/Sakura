"""Native Home scene and cache split checks; no device access."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class DeltaCyberHomeTests(unittest.TestCase):
    def test_shared_renderer(self):
        zig = next((ROOT / ".tools/zig").rglob("zig.exe"), None)
        self.assertIsNotNone(zig, "Existing native C++ compiler is required")
        with tempfile.TemporaryDirectory(prefix="delta-cyber-home-") as output:
            executable = Path(output) / "home-test.exe"
            common = [str(zig), "c++", "-std=c++11", "-O2", "-UNDEBUG", str(ROOT / "tests/delta_cyber_home_test.cpp")]
            subprocess.run(common + ["-o", str(executable)], check=True, cwd=ROOT)
            subprocess.run([str(executable)], check=True, cwd=ROOT)
            # The same shared atlas/background must also link through firmware single-TU definitions.
            subprocess.run(common + ["-DARDUINO", "-DFLUX_DELTA_BUILD", "-I", str(ROOT / "include"),
                str(ROOT / "src/core/delta_artwork.cpp"), str(ROOT / "src/core/delta_typography.cpp"),
                "-o", str(executable)], check=True, cwd=ROOT)
            subprocess.run([str(executable)], check=True, cwd=ROOT)


if __name__ == "__main__":
    unittest.main()
