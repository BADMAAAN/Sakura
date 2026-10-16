#include "../include/delta/typography.h"
#include <assert.h>
#include <stdio.h>
#include <vector>
#include <set>

int main(int argc, char **argv) {
    using namespace delta;
    using namespace delta::typeface;
    assert(lineHeight() == 18 && lineHeight(Small) == 15 && lineHeight(Numeric) == 30);
    assert(measure(nullptr) == 0 && measure("") == 0 && measure("Long label", Body, 20) == 20);
    assert(measure("AB\nignored") == measure("AB"));
    const char *utf = u8"АЁяé😀";
    assert(nextCodepoint(utf) == 0x410 && nextCodepoint(utf) == 0x401);
    assert(nextCodepoint(utf) == 0x44f && nextCodepoint(utf) == 0xe9 && nextCodepoint(utf) == 0x1f600);
    assert(nextCodepoint(utf) == 0);
    const char *malformed = "\xC0\xAF" "A\xE2";
    assert(nextCodepoint(malformed) == '?' && nextCodepoint(malformed) == 'A');
    assert(nextCodepoint(malformed) == '?' && nextCodepoint(malformed) == 0);
    assert(&glyph(0x1f600) == &glyph('?'));
    assert(&glyph(0x410) != &glyph('?'));
    assert(glyph('8', Numeric).height > glyph('8', Body).height);
    assert(sizeof(data::coverage) + sizeof(data::glyphs) + sizeof(data::codepoints) < 80000);

    std::vector<uint16_t> storage(Width * Height + 2, 0);
    storage.front() = 0x1234; storage.back() = 0x5678;
    uint16_t *frame = &storage[1];
    draw(nullptr, "safe", 0, 0, 0xffff);
    clip(frame, "outside", -1000, -1000, 10, 0xffff);
    clip(frame, u8"Wide network / Сакура", 20, 10, 84, 0xffff);
    std::set<uint16_t> shades;
    int pixels = 0;
    for (int y = 0; y < Height; ++y) for (int x = 0; x < Width; ++x) {
        uint16_t value = frame[y * Width + x];
        if (value) { assert(x >= 20 && x < 104); ++pixels; shades.insert(value); }
    }
    assert(pixels > 100 && shades.size() >= 10); // Subpixel coverage survives RGB565 conversion.
    draw(frame, "Clipped", 313, 164, 0xffff, Numeric);
    assert(storage.front() == 0x1234 && storage.back() == 0x5678);
    for (int f = 0; f < 3; ++f) for (unsigned i = 0; i < data::GlyphCount; ++i) {
        const data::Glyph &g = data::glyphs[f][i];
        assert(g.advance > 0 && g.width < 40 && g.height < 40);
        assert(g.offset + g.width * g.height <= sizeof(data::coverage) * 2);
    }
    if (argc > 1) {
        for (int i = 0; i < Width * Height; ++i) frame[i] = color(14, 15, 22);
        draw(frame, "WI-FI ANALYZER", 12, 5, color(242, 192, 215));
        draw(frame, "FLUX Network", 12, 31, color(240, 235, 242));
        draw(frame, u8"Сакура / Réseau", 12, 51, color(240, 235, 242));
        draw(frame, "CH 6  /  WPA2  /  2.4 GHz", 12, 76, color(174, 157, 188), Small);
        draw(frame, "-67", 12, 102, color(244, 165, 208), Numeric);
        draw(frame, "dBm", 68, 115, color(174, 157, 188), Small);
        draw(frame, "14px native AA / 18px line", 12, 145, color(240, 235, 242));
        FILE *file = fopen(argv[1], "wb"); assert(file);
        fprintf(file, "P6\n%d %d\n255\n", Width, Height);
        for (int i = 0; i < Width * Height; ++i) {
            uint16_t value = frame[i];
            unsigned char rgb[3] = {static_cast<unsigned char>((value >> 11) * 255 / 31), static_cast<unsigned char>(((value >> 5) & 63) * 255 / 63), static_cast<unsigned char>((value & 31) * 255 / 31)};
            fwrite(rgb, 1, 3, file);
        }
        fclose(file);
    }
    puts("Native typography: UTF-8, AA coverage, clip boundaries, atlas bounds and 80KB budget PASS");
}
