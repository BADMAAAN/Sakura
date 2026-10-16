#include "powerSave.h"
#include "display.h"
#include "settings.h"
#include "flux_power_policy.h"
#ifdef FLUX_DELTA_BUILD
#include "delta_ui.h"
#endif

void fadeOutScreen(int startValue) {
    for (int brightValue = startValue; brightValue >= 0; brightValue -= 1) {
        setBrightness(max(brightValue, 0), false);
        delay(5);
    }
    turnOffDisplay();
}

void checkPowerSaveTime() {
    unsigned long elapsed = millis() - previousMillis;
    int startDimmerBright = fluxConfig.bright / 3;
    const auto action = fluxIdleAction(elapsed, fluxConfig.dimmerSet, dimmer, isScreenOff, isSleeping);
    if (action == FluxIdleAction::Dim) {
        dimmer = true;
        setBrightness(startDimmerBright, false);
    } else if (action == FluxIdleAction::DisplayOff) {
        isScreenOff = true;
        fadeOutScreen(startDimmerBright);
    }
}

void sleepModeOn() {
    isSleeping = true;
    isScreenOff = true;
#ifdef FLUX_DELTA_BUILD
    setBrightness(0, false);
    deltaUiInvalidate();
#else
    int startDimmerBright = fluxConfig.bright / 3;
    fadeOutScreen(startDimmerBright);
#endif
    panelSleep(true); //  power down screen
    setCpuFrequencyMhz(80);

    delay(200);
}

void sleepModeOff() {
#ifdef FLUX_DELTA_BUILD
    deltaUiInvalidate();
#endif
    isSleeping = false;
    isScreenOff = false;
    dimmer = false;
    previousMillis = millis();
    SelPress = NextPress = PrevPress = EscPress = AnyKeyPress = false;
#ifdef HAS_ENCODER
    RotaryNetSteps = 0;
#endif
    setCpuFrequencyMhz(CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ);

    panelSleep(false); // wake the screen back up
#ifdef FLUX_DELTA_BUILD
    deltaUiRestore(); // Present a complete frame while the backlight is still off.
#endif
    getBrightness();
    delay(200);
}
