#include "delta/rf_graph.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <vector>

int main() {
    using namespace delta;
    using namespace delta::ui;
    std::vector<uint16_t> guarded(Width * Height + 2, 0xA55A), empty(Width * Height);
    uint16_t *p = guarded.data() + 1;
    Appearance a;
    const float frequencies[] = {433.05f, 433.20f, 434.79f};
    int16_t values[] = {RssiUnmeasured, RssiUnmeasured, RssiUnmeasured};
    rfGraph(empty.data(), a, values, frequencies, 3);
    rfGraph(p, a, nullptr, frequencies, 3);
    assert(std::equal(empty.begin(), empty.end(), p));
    values[1] = -61;
    rfGraph(p, a, values, frequencies, 3);
    assert(!std::equal(empty.begin(), empty.end(), p));
    assert(rfSampleX(frequencies, 3, 1) < 100); // Real nonuniform MHz positions.
    assert(rfDbmY(-200) == RfBottom && rfDbmY(0) == RfTop);
    assert(rfSampleX(nullptr, 1, 0) == (RfLeft + RfRight) / 2);
    for (Theme theme : {Theme::DarkSakura, Theme::SakuraDawn, Theme::Monochrome}) {
        a.theme = theme;
        int16_t extremes[] = {-200, -20, RssiUnmeasured, -127};
        rfGraph(p, a, extremes, nullptr, 4, 433.92f);
        rfGraph(p, a, extremes, nullptr, 1, 433.92f);
        rfGraph(p, a, nullptr, nullptr, 0);
        rfGraph(nullptr, a, extremes, nullptr, 4);
        assert(guarded.front() == 0xA55A && guarded.back() == 0xA55A);
    }
}
