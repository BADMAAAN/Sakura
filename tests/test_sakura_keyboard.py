"""Actual board input + shared keyboard; inert display, power and network doubles."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest
from test_sakura_back import function

ROOT = Path(__file__).resolve().parents[1]


class SakuraKeyboardTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        def read(path):
            if os.environ.get('SAKURA_TEST_HEAD'):
                return subprocess.check_output(['git', 'show', 'HEAD:' + path], cwd=ROOT).decode('utf-8')
            return (ROOT / path).read_text(encoding='utf-8')
        keyboard = read('src/core/mykeyboard.cpp')
        globals_source = read('include/globals.h')
        source = (ROOT / 'tests/sakura_keyboard_test.cpp').read_text(encoding='utf-8')
        layouts = keyboard[keyboard.index('// QWERTY KEYSET'):keyboard.index('#if defined(HAS_TOUCH)')]
        handlers = '\n'.join(function(keyboard, signature) for signature in (
            'bool handleDelete(', 'bool handleCharacterAdd(', 'bool handleSpaceAdd(',
            'KeyboardAction handleKeyboardSelection('))
        start = keyboard.index('String generalKeyboard(')
        controller = 'template <int KeyboardHeight, int KeyboardWidth>\n' + keyboard[start:keyboard.index('/// Opens a simple option menu', start)]
        source = source.replace('/* CHECK */', function(globals_source, 'extern inline bool check('))
        source = source.replace('/* CLEAR */', function(globals_source, 'extern inline void clearInputState()'))
        source = source.replace('/* BOARD */', function(read('boards/lilygo-t-embed-cc1101/interface.cpp'), 'void InputHandler(void)'))
        source = source.replace('/* KEYBOARD */', layouts + '\n' + handlers + '\n' + controller)
        source = source.replace('/* WIFI */', function(read('src/core/wifi/wifi_common.cpp'), 'bool _wifiConnect('))
        cls.directory = tempfile.TemporaryDirectory(prefix='sakura-keyboard-')
        cls.exe = Path(cls.directory.name) / 'keyboard.exe'
        zig = next((ROOT / '.tools/zig').rglob('zig.exe'))
        result = subprocess.run([str(zig), 'c++', '-std=c++11', '-Wno-c++11-narrowing', '-O1', '-UNDEBUG', '-I', str(ROOT / 'include'),
                                 '-I', str(ROOT / 'src'), '-x', 'c++', '-', '-o', str(cls.exe)],
                                input=source, text=True, encoding='utf-8', capture_output=True, cwd=ROOT)
        if result.returncode:
            cls.directory.cleanup()
            raise AssertionError(result.stdout + result.stderr)

    @classmethod
    def tearDownClass(cls):
        cls.directory.cleanup()

    def test_actual_input_and_controller(self):
        for layout in ('qwerty', 'hex', 'numeric'):
            for scenario in ('reach', 'back', 'edit', 'cancel', 'reopen', 'wake', 'burst'):
                with self.subTest(layout=layout, scenario=scenario):
                    result = subprocess.run([str(self.exe), layout, scenario], capture_output=True, text=True, timeout=8)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_hold_handler_from_every_focus(self):
        for focus in range(53):
            with self.subTest(focus=focus):
                result = subprocess.run([str(self.exe), 'qwerty', 'hold', str(focus)], capture_output=True, text=True, timeout=8)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == '__main__':
    unittest.main()
