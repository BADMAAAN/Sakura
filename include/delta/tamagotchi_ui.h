#pragma once
#include "widgets.h"
#include <stdio.h>

namespace delta { namespace ui {
// Presentation only: callers provide the existing mood and measured counters.
inline void tamagotchi(uint16_t *p, const Appearance &a, const char *face, const char *phrase,
                       unsigned channel, unsigned handshakes, int battery, unsigned long uptime,
                       unsigned friendsRun, unsigned friendsTotal, const char *lastFriend,
                       const char *rssi, bool showToolbars) {
    if (!p) return;
    workBackground(p, a);
    const Palette c = palette(a.theme);
    box(p, 0, 0, Width, Height, c.background, 0, 110);
    header(p, "Sakura Tamagotchi", a);
    if (showToolbars) {
        char metrics[80];
        snprintf(metrics, sizeof(metrics), "CH %02u  HS %u    UPS %d%%  UP %02lu:%02lu:%02lu",
                 channel, handshakes, battery, uptime / 3600, uptime / 60 % 60, uptime % 60);
        typeface::clip(p, metrics, 14, 35, 292, c.text, typeface::Small);
    }
    const int faceWidth = typeface::measure(face, typeface::Numeric, 292);
    typeface::clip(p, face, (Width - faceWidth) / 2, 61, 292, c.accent, typeface::Numeric);
    const int phraseWidth = typeface::measure(phrase, typeface::Small, 292);
    typeface::clip(p, phrase, (Width - phraseWidth) / 2, 105, 292, c.text, typeface::Small);
    box(p, 14, 130, 292, 1, c.line);
    if (showToolbars) {
        char friends[48];
        snprintf(friends, sizeof(friends), "FRND %u (%u)", friendsRun, friendsTotal);
        typeface::clip(p, friends, 14, 134, 240, c.text, typeface::Small);
        if (friendsRun) {
            char peer[64];
            snprintf(peer, sizeof(peer), "[%.13s] %s", lastFriend, rssi);
            typeface::clip(p, peer, 14, 151, 242, c.text, typeface::Small);
        }
    }
    typeface::clip(p, "NOT AI", 266, 151, 48, c.accent, typeface::Small);
}
}} // namespace delta::ui
