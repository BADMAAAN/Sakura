from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class DeltaSettingsTests(unittest.TestCase):
    def test_portable_settings_roundtrip_migration_and_rejection(self):
        compiler = next((ROOT / '.tools').rglob('zig.exe'), None)
        command = [str(compiler), 'c++'] if compiler else [shutil.which('g++') or 'g++']
        with tempfile.TemporaryDirectory() as temp:
            exe = Path(temp) / 'delta_settings_test.exe'
            subprocess.run(command + ['-std=c++11', '-O2', '-UNDEBUG', str(ROOT / 'tests/delta_settings_test.cpp'), '-o', str(exe)], check=True, cwd=ROOT)
            subprocess.run([str(exe)], check=True, cwd=ROOT)

if __name__ == '__main__':
    unittest.main()
