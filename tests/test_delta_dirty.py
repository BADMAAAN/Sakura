"""Pixel-exact LCD emulator for full-width changed-row transfers."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]

class DeltaDirtyTests(unittest.TestCase):
    def test_changed_row_transfers_match_full_frame(self):
        zig=next((ROOT/'.tools/zig').rglob('zig.exe'))
        with tempfile.TemporaryDirectory() as folder:
            exe=Path(folder)/'dirty.exe'
            subprocess.run([str(zig),'c++','-std=c++11','-O2','-UNDEBUG','-I',str(ROOT/'include'),str(ROOT/'tests/delta_dirty_test.cpp'),'-o',str(exe)],check=True,cwd=ROOT)
            subprocess.run([str(exe)],check=True,cwd=ROOT)

if __name__=='__main__':unittest.main()
