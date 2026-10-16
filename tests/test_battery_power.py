"""Run the same portable battery and idle policy used by the firmware."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class BatteryPowerTests(unittest.TestCase):
    def compile_run(self, source):
        zig = next((ROOT / ".tools/zig").rglob("zig.exe"), None)
        compiler = os.environ.get("CXX") or shutil.which("g++") or shutil.which("clang++")
        command = [compiler] if compiler else [str(zig), "c++"]
        if compiler and Path(compiler).stem == "zig":
            command += ["c++"]
        self.assertTrue(compiler or zig, "A host C++ compiler is required")
        with tempfile.TemporaryDirectory(prefix="flux-battery-") as folder:
            exe = Path(folder) / "battery_power.exe"
            fixture = Path(folder) / "check.cpp"
            fixture.write_text(source, encoding="utf-8")
            subprocess.run(command + ["-std=c++11", "-I", str(ROOT / "include"),
                           str(fixture), "-o", str(exe)], check=True)
            subprocess.run([str(exe)], check=True)

    def test_compiled_policy(self):
        self.compile_run((ROOT / "tests/battery_power_test.cpp").read_text())

    def test_actual_software_sleep_and_restore(self):
        source = r'''
#include <assert.h>
#include <stdint.h>
#include "flux_power_policy.h"
#define HAS_ENCODER 1
#define FLUX_DELTA_BUILD 1
#define CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ 240
struct Config { int bright = 90, dimmerSet = 10; } fluxConfig;
uint32_t now = 100, previousMillis = 0;
bool isSleeping = false, isScreenOff = false, dimmer = false;
bool SelPress = true, NextPress = true, PrevPress = true, EscPress = true, AnyKeyPress = true;
int RotaryNetSteps = 5, cpu = 240, bright = 90;
bool panelAsleep = false, restored = false;
uint32_t millis() { return now; }
void delay(int ms) { now += ms; }
int max(int a, int b) { return a > b ? a : b; }
void setBrightness(int b, bool) { bright = b; }
void turnOffDisplay() { bright = 0; panelAsleep = true; }
void panelSleep(bool state) { assert(bright == 0); panelAsleep = state; }
void deltaUiInvalidate() { restored = false; }
bool deltaUiRestore() { assert(!panelAsleep && bright == 0); restored = true; return true; }
void getBrightness() { assert(restored); bright = fluxConfig.bright; }
void setCpuFrequencyMhz(int mhz) { cpu = mhz; }
'''
        source += "\n".join(line for line in (ROOT / "src/core/powerSave.cpp").read_text().splitlines()
                            if not line.startswith("#include"))
        source += r'''
int main() {
    sleepModeOn();
    assert(isSleeping && isScreenOff && panelAsleep && bright == 0 && cpu == 80);
    sleepModeOff();
    assert(!isSleeping && !isScreenOff && !panelAsleep && !dimmer);
    assert(bright == 90 && cpu == 240 && previousMillis > 0);
    assert(!SelPress && !NextPress && !PrevPress && !EscPress && !AnyKeyPress && RotaryNetSteps == 0);
    checkPowerSaveTime();
    assert(!isScreenOff && !dimmer); // No immediate return to sleep from the old timer.
    now = previousMillis + 15000;
    checkPowerSaveTime();
    assert(isScreenOff && panelAsleep && !isSleeping);
}
'''
        self.compile_run(source)

    def test_deep_sleep_blanks_before_shutdown(self):
        board = (ROOT / "boards/lilygo-t-embed-cc1101/interface.cpp").read_text()
        startup = board.split("void _setup_gpio()", 1)[1].split("// Power chip pin", 1)[0]
        self.assertLess(startup.index("digitalWrite(TFT_BL, LOW)"), startup.index("digitalWrite(PIN_POWER_ON, HIGH)"))
        transition = board.split("// Execute deep sleep", 1)[1].split("esp_deep_sleep_start();", 1)[0]
        self.assertNotIn("fillScreen", transition)
        self.assertLess(transition.index("isScreenOff = true"), transition.index("setBrightness(0, false)"))
        self.assertLess(transition.index("setBrightness(0, false)"), transition.index("panelSleep(true)"))
        self.assertLess(transition.index("panelSleep(true)"), transition.index("WiFi.disconnect"))

    def test_actual_wake_button_is_consumed_until_release(self):
        source = r'''
#include <assert.h>
#include <stdint.h>
#define T_EMBED_1101 1
#define SEL_BTN 1
#define BK_BTN 2
#define BTN_ACT 0
uint32_t now = 100;
uint32_t millis() { return now; }
bool isSleeping = false, DiscardEncoderInput = false, LongPress = false;
bool SelPress = false, NextPress = false, PrevPress = false, EscPress = false, AnyKeyPress = false;
int RotaryNetSteps = 0, button = 1;
int digitalRead(int pin) { return pin == SEL_BTN ? button : 1; }
bool wakeUpScreen() { return false; }
struct Encoder { int getPosition() { return 0; } } input;
Encoder *encoder = &input;
'''
        board = (ROOT / "boards/lilygo-t-embed-cc1101/interface.cpp").read_text()
        source += board[board.index("void InputHandler(void)"):board.index("\nvoid powerOff()")]
        source += r'''
int main() {
    InputHandler();
    now += 250; isSleeping = true; button = 0;
    InputHandler();
    assert(AnyKeyPress && !SelPress && !EscPress && RotaryNetSteps == 0);
    isSleeping = false; AnyKeyPress = false;
    now += 250; InputHandler();
    assert(!SelPress && !AnyKeyPress); // A held wake key cannot open the selected app.
    button = 1; InputHandler();
    now += 250; button = 0; InputHandler();
    assert(SelPress); // A fresh press still works normally.
}
'''
        self.compile_run(source)

    def test_actual_ble_task_keeps_unknown_out_of_battery_level(self):
        source = r'''
#include <assert.h>
#include <stdint.h>
#include <setjmp.h>
jmp_buf finish;
int sampleIndex = 0, ticks = 0;
int getBattery() { const int samples[] = {-1, 77, -1}; return samples[sampleIndex++]; }
struct NimBLECharacteristic {
    int writes = 0, level = -1;
    void setValue(const uint8_t *value, int count) { assert(count == 1); level = *value; ++writes; }
};
void delay(int) { if (++ticks == 3) longjmp(finish, 1); }
'''
        service = (ROOT / "src/modules/ble_api/services/BatteryService.cpp").read_text()
        start = service.index("void battery_handler_task(")
        source += service[start:service.index("\n}\n", start) + 3]
        source += r'''
int main() {
    static NimBLECharacteristic characteristic;
    if (setjmp(finish) == 0) battery_handler_task(&characteristic);
    assert(characteristic.writes == 1 && characteristic.level == 77);
}
'''
        self.compile_run(source)


if __name__ == "__main__":
    unittest.main()
