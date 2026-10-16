#include <assert.h>
#include "flux_battery_filter.h"
#include "flux_power_policy.h"

int main() {
    FluxBatteryFilter battery;
    assert(battery.level() == -1);
    for (int i = 0; i < 4; ++i) battery.sample(true, 0, 4000, 8, 0);
    assert(battery.level() == -1); // Unknown remains unknown; no fabricated percentage.
    battery.sample(false, 0, 0, 0, 0x20);
    assert(battery.level() == -1);
    for (int i = 0; i < 3; ++i) battery.sample(true, 80, 4000, 8, 0x20);
    assert(battery.level() == 80);
    for (int i = 0; i < 4; ++i) {
        battery.sample(true, 0, 4000, 8, 0); // INITCOMP is not set after gauge reset.
        battery.sample(true, 0, 4000, 8, 0x420); // Gauging suspended for configuration.
        battery.sample(true, 0, 4000, 8, 0x21); // Calibration mode.
        battery.sample(true, 0, 4000, 8, 0xffff); // Invalid operation status.
    }
    assert(battery.level() == 80); // Not-ready gauge must never turn a charged battery red.
    battery.sample(true, 0, 4000, 8, 0x20);
    assert(battery.level() == 80); // A single valid-looking bad SOC cannot trigger low battery.
    battery.sample(true, 79, 4000, 8, 0x20);
    battery.sample(false, 0, 0, 0, 0x20);
    battery.sample(true, 65535, 4000, 8, 0x20);
    battery.sample(true, 0, 0, 8, 0x20);
    battery.sample(true, 0, 4000, 0, 0x20);
    assert(battery.level() == 79);
    for (int i = 0; i < 3; ++i) battery.sample(true, 9, 3100, 8, 0x20);
    assert(battery.level() == 9); // Sustained real low readings are not hidden.
    for (int i = 0; i < 3; ++i) battery.sample(true, 16, 3500, 8, 0x20);
    battery.sample(true, 15, 3500, 8, 0x20);
    assert(battery.level() == 16); // The existing red-battery threshold is below 16%.
    assert(!fluxChargerCharging(0x02));
    assert(fluxChargerCharging(0x2e)); // USB power good, precharge.
    assert(fluxChargerCharging(0x36)); // USB power good, fast charge.
    assert(!fluxChargerCharging(0x3e)); // Charge complete.
    assert(!fluxChargerCharging(0xff)); // OTG/invalid is not charging.
    assert(fluxIdleAction(9999, 10, false, false, false) == FluxIdleAction::None);
    assert(fluxIdleAction(10000, 10, false, false, false) == FluxIdleAction::Dim);
    assert(fluxIdleAction(15000, 10, true, false, false) == FluxIdleAction::DisplayOff);
    assert(fluxIdleAction(20000, 10, true, true, false) == FluxIdleAction::None);
    assert(fluxIdleAction(20000, 10, false, false, true) == FluxIdleAction::None);
    assert(fluxIdleAction(20000, 0, false, false, false) == FluxIdleAction::None);
}
