"""One portable compiled check for firmware navigation/render semantics."""
from pathlib import Path
import subprocess
import unittest

ROOT=Path(__file__).resolve().parents[1]
class DeltaCoreTests(unittest.TestCase):
    def test_compiled_controller_and_renderer(self):
        zig=next((ROOT/".tools/zig").rglob("zig.exe"),None)
        self.assertIsNotNone(zig,"Portable Zig is required for the shared renderer check")
        exe=ROOT/"tools/delta_preview/delta_core_test.exe"
        subprocess.run([str(zig),"c++","-std=c++11","-O2","-UNDEBUG",str(ROOT/"tests/delta_core_test.cpp"),"-o",str(exe)],check=True,cwd=ROOT)
        subprocess.run([str(exe)],check=True,cwd=ROOT)
if __name__=="__main__":unittest.main()
