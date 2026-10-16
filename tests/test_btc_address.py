"""Run the production Base58Check validator with Python's SHA-256 oracle."""
import ctypes
import hashlib
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
HASH = ctypes.CFUNCTYPE(ctypes.c_bool, ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t,
                       ctypes.POINTER(ctypes.c_uint8))


class BitcoinTests(unittest.TestCase):
    def test_mainnet_checksum_and_rejection(self):
        zig = next((ROOT / '.tools/zig').rglob('zig.exe'))
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / 'check.cpp').write_text('''#include "btc_address.h"
extern "C" __declspec(dllexport) bool check(const char *s, sakura::Hash256 hash) {
 return sakura::validBitcoinAddress(s, hash);
}''')
            subprocess.run([str(zig), 'c++', '-std=c++11', '-O2', '-shared',
                            '-I', str(ROOT / 'include'), str(path / 'check.cpp'),
                            '-o', str(path / 'check.dll')], check=True)
            library = ctypes.CDLL(str(path / 'check.dll'))
            library.check.argtypes = [ctypes.c_char_p, HASH]
            library.check.restype = ctypes.c_bool

            @HASH
            def sha(data, length, output):
                ctypes.memmove(output, hashlib.sha256(ctypes.string_at(data, length)).digest(), 32)
                return True

            # Published Bitcoin examples; never preconfigure these as the user's wallet.
            for address in (b'1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa',
                            b'3J98t1WpEZ73CNmQviecrnyiWrnqRhWNLy'):
                self.assertTrue(library.check(address, sha))
                self.assertFalse(library.check(address[:-1] + b'1', sha))
            for address in (b'', b'1' * 26, b'1' * 35, b'0OIl', b' ' + b'1' * 33,
                            b'mipcBbFg9gMiCh81Kj8tqqdgoZub1ZJRfn',
                            b'bitcoin:1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa', b'bc1qinvalid'):
                self.assertFalse(library.check(address, sha), address)
            self.assertFalse(library.check(b'1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa',
                                           HASH(lambda *_: False)))
            # Windows keeps DLL handles open until explicitly unloaded.
            ctypes.windll.kernel32.FreeLibrary(ctypes.c_void_p(library._handle))


if __name__ == '__main__':
    unittest.main()
