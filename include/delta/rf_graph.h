#pragma once
#include "instruments.h"

namespace delta { namespace ui {
static const int16_t RssiUnmeasured = Unmeasured;
static const int RfLeft = 38, RfRight = 306, RfTop = 89, RfBottom = 136;
inline int rfDbmY(int dbm) {
    return RfBottom - (bounded(dbm, -110, -20) + 110) * (RfBottom - RfTop) / 90;
}
inline int rfSampleX(const float *frequencies, int count, int index) {
    if (count <= 1) return (RfLeft + RfRight) / 2;
    if (frequencies && frequencies[count - 1] > frequencies[0])
        return bounded(RfLeft + int((frequencies[index] - frequencies[0]) *
                       (RfRight - RfLeft) / (frequencies[count - 1] - frequencies[0])), RfLeft, RfRight);
    return RfLeft + index * (RfRight - RfLeft) / (count - 1);
}
// Frequencies are actual scan bins. A null frequency array means chronological history.
inline void rfGraph(uint16_t *p, const Appearance &a, const int16_t *samples,
                    const float *frequencies, int count, float fixedMHz = 0) {
    if (!p) return;
    workBackground(p, a);
    const Palette c = palette(a.theme);
    modeHeader(p, "RF ANALYZER", frequencies ? "SPECTRUM / RX" : "SIGNAL HISTORY / RX", a);
    const int ticks[] = {-20, -50, -80, -110};
    for (int dbm : ticks) {
        const int y = rfDbmY(dbm);
        line(p, RfLeft, y, RfRight, y, blend(c.background, c.line, 90));
        char label[8]; snprintf(label, sizeof(label), "%d", dbm);
        typeface::clip(p, label, 3, y - 7, 31, c.muted, typeface::Small);
    }
    for (int i = 0; i <= 4; ++i) {
        int x = RfLeft + (RfRight - RfLeft) * i / 4;
        line(p, x, RfTop, x, RfBottom, blend(c.background, c.line, 65));
    }
    int peak = -1, measured = 0, previous = -1;
    if (samples && count > 0) for (int i = 0; i < count; ++i) {
        if (samples[i] == RssiUnmeasured) { previous = -1; continue; }
        ++measured;
        if (peak < 0 || samples[i] > samples[peak]) peak = i;
        const int x = rfSampleX(frequencies, count, i), y = rfDbmY(samples[i]);
        if (previous >= 0) {
            const int px = rfSampleX(frequencies, count, previous), py = rfDbmY(samples[previous]);
            // A single clean trace keeps measured history legible without repeated alpha overdraw.
            line(p, px, py, x, y, c.accent);
        } else pixel(p, x, y, c.accent);
        previous = i;
    }
    char label[72];
    if (peak >= 0) {
        snprintf(label, sizeof(label), "%.3f MHz", double(frequencies ? frequencies[peak] : fixedMHz));
        const int x = rfSampleX(frequencies, count, peak), y = rfDbmY(samples[peak]);
        for (int yy = RfTop; yy < RfBottom; yy += 5) pixel(p, x, yy, c.muted, 100);
        box(p, x - 1, y - 1, 3, 3, c.text, 1);
    } else if (!frequencies && fixedMHz > 0) snprintf(label, sizeof(label), "%.3f MHz", double(fixedMHz));
    else snprintf(label, sizeof(label), "RF receiver");
    textClip(p, label, 14, 61, 166, c.text);
    typeface::clip(p, "PEAK", 176, 65, 40, c.muted, typeface::Small);
    if (peak >= 0) snprintf(label, sizeof(label), "%d", int(samples[peak]));
    else snprintf(label, sizeof(label), "--");
    typeface::clip(p, label, 216, 55, 63, c.accent, typeface::Numeric);
    typeface::clip(p, "dBm", 280, 70, 27, c.muted, typeface::Small);
    if (frequencies && count > 0) {
        snprintf(label, sizeof(label), "%.3f", double(frequencies[0]));
        typeface::clip(p, label, RfLeft, 139, 78, c.muted, typeface::Small);
        snprintf(label, sizeof(label), "%.3f MHz", double(frequencies[count - 1]));
        int w = typeface::measure(label, typeface::Small, 100); typeface::clip(p, label, RfRight - w, 139, w, c.muted, typeface::Small);
    } else {
        typeface::clip(p, "OLDER", RfLeft, 139, 70, c.muted, typeface::Small);
        const int nowWidth = typeface::measure("NOW", typeface::Small);
        typeface::clip(p, "NOW", RfRight - nowWidth, 139, nowWidth, c.muted, typeface::Small);
    }
    snprintf(label, sizeof(label), measured ? "RX / %d measured %s" : "RX / awaiting measured RSSI", measured, frequencies ? "bins" : "samples");
    typeface::clip(p, label, 14, 155, 292, c.muted, typeface::Small);
}
}} // namespace delta::ui
