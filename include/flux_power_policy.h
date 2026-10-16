#pragma once
#include <stdint.h>

enum class FluxIdleAction { None, Dim, DisplayOff };
inline FluxIdleAction fluxIdleAction(uint32_t elapsed, unsigned dimmerSeconds, bool dimmed,
                                    bool displayOff, bool sleeping) {
    if (!dimmerSeconds || displayOff || sleeping) return FluxIdleAction::None;
    const uint32_t timeout = dimmerSeconds * 1000U;
    if (elapsed >= timeout + 5000U) return FluxIdleAction::DisplayOff;
    if (elapsed >= timeout && !dimmed) return FluxIdleAction::Dim;
    return FluxIdleAction::None;
}
