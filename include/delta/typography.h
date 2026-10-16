#pragma once
#include "core.h"
#include "typography_data.h"

// Native raster sizes for interior screens. Home keeps its original artwork font.
namespace delta { namespace typeface {
enum Face : uint8_t { Body, Small, Numeric };
inline int lineHeight(Face face = Body) { return face == Numeric ? 30 : face == Small ? 15 : 18; }

// Consume one Unicode scalar, replacing a malformed byte sequence with one '?'.
inline uint32_t nextCodepoint(const char *&text) {
    if (!text || !*text) return 0;
    unsigned char lead = static_cast<unsigned char>(*text++);
    if (lead < 0x80) return lead;
    int continuation = lead >= 0xC2 && lead <= 0xDF ? 1 : lead >= 0xE0 && lead <= 0xEF ? 2 : lead >= 0xF0 && lead <= 0xF4 ? 3 : 0;
    if (!continuation) {
        while ((static_cast<unsigned char>(*text) & 0xC0) == 0x80) ++text;
        return '?';
    }
    uint32_t codepoint = lead & ((1u << (6 - continuation)) - 1);
    for (int i = 0; i < continuation; ++i) {
        unsigned char next = static_cast<unsigned char>(*text);
        if ((next & 0xC0) != 0x80) return '?';
        ++text;
        codepoint = codepoint << 6 | (next & 63);
    }
    if ((continuation == 1 && codepoint < 0x80) || (continuation == 2 && codepoint < 0x800) ||
        (continuation == 3 && codepoint < 0x10000) || codepoint > 0x10FFFF || (codepoint >= 0xD800 && codepoint <= 0xDFFF)) return '?';
    return codepoint;
}

inline const data::Glyph &glyph(uint32_t codepoint, Face face = Body) {
    unsigned low = 0, high = data::GlyphCount;
    while (low < high) {
        unsigned middle = (low + high) / 2;
        if (data::codepoints[middle] < codepoint) low = middle + 1;
        else high = middle;
    }
    if (low == data::GlyphCount || data::codepoints[low] != codepoint) low = '?' - 32;
    return data::glyphs[face <= Numeric ? unsigned(face) : unsigned(Body)][low];
}

inline int measure(const char *text, Face face = Body, int maxWidth = Width) {
    if (!text || maxWidth <= 0) return 0;
    int width = 0;
    while (*text) {
        uint32_t codepoint = nextCodepoint(text);
        if (codepoint == '\n' || codepoint == '\r') break;
        int advance = glyph(codepoint, face).advance;
        if (advance > maxWidth - width) return maxWidth;
        width += advance;
    }
    return width;
}

inline void drawGlyph(uint16_t *frame, uint32_t codepoint, int x, int y, uint16_t color, Face face,
                      unsigned opacity, int clipLeft, int clipRight) {
    const data::Glyph &g = glyph(codepoint, face);
    for (int row = 0; row < g.height; ++row) {
        int yy = y + g.y + row;
        if (unsigned(yy) >= Height) continue;
        for (int column = 0; column < g.width; ++column) {
            int xx = x + g.x + column;
            if (xx < clipLeft || xx >= clipRight || unsigned(xx) >= Width) continue;
            unsigned alpha = delta::alpha(data::coverage, g.offset + unsigned(row * g.width + column));
            if (alpha) delta::pixel(frame, xx, yy, color, alpha * 17 * opacity / 255);
        }
    }
}

inline void draw(uint16_t *frame, const char *text, int x, int y, uint16_t color, Face face = Body, unsigned opacity = 255) {
    if (!frame || !text || !opacity) return;
    if (opacity > 255) opacity = 255;
    while (*text && x < Width) {
        uint32_t codepoint = nextCodepoint(text);
        if (codepoint == '\n' || codepoint == '\r') break;
        drawGlyph(frame, codepoint, x, y, color, face, opacity, 0, Width);
        x += glyph(codepoint, face).advance;
    }
}

inline void clip(uint16_t *frame, const char *text, int x, int y, int width, uint16_t color,
                 Face face = Body, unsigned opacity = 255) {
    if (!frame || !text || width <= 0 || !opacity || x >= Width) return;
    if (opacity > 255) opacity = 255;
    if (width > Width) width = Width;
    int end = x + width;
    bool truncated = measure(text, face, width + 1) > width;
    int ellipsis = glyph('.', face).advance * 3;
    int limit = truncated ? end - ellipsis : end;
    int cursor = x;
    while (*text && cursor < limit) {
        uint32_t codepoint = nextCodepoint(text);
        if (codepoint == '\n' || codepoint == '\r') break;
        const data::Glyph &g = glyph(codepoint, face);
        if (cursor + g.advance > limit) break;
        drawGlyph(frame, codepoint, cursor, y, color, face, opacity, x, end);
        cursor += g.advance;
    }
    if (truncated) for (int i = 0; i < 3; ++i) {
        drawGlyph(frame, '.', cursor, y, color, face, opacity, x, end);
        cursor += glyph('.', face).advance;
    }
}
} } // namespace delta::typeface
