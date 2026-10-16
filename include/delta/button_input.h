#pragma once
#include <stdint.h>

namespace delta {
// Stable edges, not a repeating level: a held button belongs to one screen.
struct ButtonInput {
    bool raw = false, down = false;
    uint32_t changedAt = 0;
    int sample(bool pressed, uint32_t now) {
        if (pressed != raw) { raw = pressed; changedAt = now; }
        if (down == raw || uint32_t(now - changedAt) < 20) return 0;
        down = raw;
        return down ? 1 : -1;
    }
};
}
