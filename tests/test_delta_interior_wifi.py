"""Exercise the physical firmware's portable Wi-Fi painters without radio activity."""
from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]


class DeltaInteriorWifiTests(unittest.TestCase):
    def test_shared_native_wifi_painters(self):
        zig = next((ROOT / ".tools/zig").rglob("zig.exe"), None)
        self.assertIsNotNone(zig, "Use the existing portable compiler")
        executable = ROOT / "tools/delta_preview/delta_interior_wifi_test.exe"
        subprocess.run([str(zig), "c++", "-std=c++11", "-O2", "-UNDEBUG",
                        str(ROOT / "tests/delta_interior_wifi_test.cpp"), "-o", str(executable)],
                       check=True, cwd=ROOT)
        subprocess.run([str(executable)], check=True, cwd=ROOT)


if __name__ == "__main__":
    unittest.main()
