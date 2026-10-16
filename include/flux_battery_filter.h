#pragma once
#include <stdint.h>
#include <atomic>

class FluxBatteryFilter {
    std::atomic<int> accepted{-1};
    int candidate = -1;
    unsigned confirmations = 0;

public:
    int level() const { return accepted.load(); }
    void sample(bool transportOk, int soc, int millivolts, uint16_t status, uint16_t operationStatus) {
        // BQ27220 TRM 2.27: INITCOMP must be set; CFGUPDATE/CALMD suspend normal gauging.
        if (!transportOk || soc < 0 || soc > 100 || millivolts < 2500 || millivolts > 4500 ||
            !(status & (1U << 3)) || status == 0xffff ||
            (operationStatus & ((1U << 5) | (1U << 10) | 1U)) != (1U << 5)) {
            confirmations = 0;
            candidate = -1;
            return;
        }
        // Reject one-off startup/large changes and low-battery threshold crossings.
        const int delta = soc - accepted;
        if (accepted >= 0 && delta >= -5 && delta <= 5 && !(accepted >= 16 && soc < 16)) {
            accepted = soc;
            confirmations = 0;
            candidate = -1;
            return;
        }
        if (candidate < 0 || soc < candidate - 2 || soc > candidate + 2) confirmations = 0;
        candidate = soc;
        if (++confirmations >= 3) {
            accepted = soc;
            confirmations = 0;
            candidate = -1;
        }
    }
};

inline bool fluxChargerCharging(uint8_t status) {
    // TI BQ25896 REG0B: VBUS[7:5], CHRG_STAT[4:3], PG_STAT[2].
    const unsigned vbus = status >> 5;
    const unsigned phase = (status >> 3) & 3;
    return (status & 4) && (vbus == 1 || vbus == 2) && (phase == 1 || phase == 2);
}
