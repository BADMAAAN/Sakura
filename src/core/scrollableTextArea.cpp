#include "scrollableTextArea.h"
#include "delta_ui.h"
#define _scrollBuffer tft
ScrollableTextArea::ScrollableTextArea(const String &title)
    : firstVisibleLine{0}, lastVisibleLine{0}, _deltaStandard{true}, _redraw{true}, _title(title), _fontSize(FP), _startX(BORDER_PAD_X),
      _startY(BORDER_PAD_Y), _width(tftWidth - 2 * BORDER_PAD_X),
      _height(tftHeight - 4 - BORDER_PAD_X - BORDER_PAD_Y), _indentWrappedLines{false} {
    drawMainBorder();

    if (!_title.isEmpty()) {
        printTitle(_title);
        _startY = tft.getCursorY();
        _height -= (_startY - BORDER_PAD_Y);
    }
#ifdef FLUX_DELTA_BUILD
    _startX = 14;
    _startY = 37;
    _width = 292;
    _height = 128;
#endif

    setup();
}

ScrollableTextArea::ScrollableTextArea(
    uint8_t fontSize, int16_t startX, int16_t startY, int32_t width, int32_t height, bool drawBorders,
    bool indentWrappedLines
)
    : firstVisibleLine{0}, lastVisibleLine{0}, _deltaStandard{false}, _redraw{true}, _title(""), _fontSize(fontSize), _startX(startX), _startY(startY),
      _width(width), _height(height), _indentWrappedLines(indentWrappedLines) {
    if (drawBorders) { drawMainBorder(); }
    setup();
}

ScrollableTextArea::~ScrollableTextArea() {
    // We don't use Sprites for big things, unfortunetly theres no much RAM in all devices
}

void ScrollableTextArea::setup() {
    _scrollBuffer.setTextColor(fluxConfig.priColor);
    _scrollBuffer.setTextSize(_fontSize);
    _scrollBuffer.fillRect(_startX, _startY, _width, _height, fluxConfig.bgColor);

    int characterWidth = _scrollBuffer.textWidth("w", _fontSize);
    if (characterWidth < 1) characterWidth = 1;
    _maxCharactersPerLine = _width > 0 ? _width / characterWidth : 1;
    if (_maxCharactersPerLine < 1) _maxCharactersPerLine = 1;
    _pixelsPerLine = _scrollBuffer.fontHeight() + 2;
#ifdef FLUX_DELTA_BUILD
    if (_deltaStandard) _pixelsPerLine = delta::typeface::lineHeight() + 2;
#endif
    if (_pixelsPerLine < 1) _pixelsPerLine = 1;
    _maxVisibleLines = _height > 0 ? _height / _pixelsPerLine : 1;
    if (_maxVisibleLines < 1) _maxVisibleLines = 1;
}

void ScrollableTextArea::scrollUp() {
    if (firstVisibleLine) {
        firstVisibleLine--;
        _redraw = true;
    }
}

void ScrollableTextArea::scrollDown() {
    if (linesBuffer.size() > _maxVisibleLines && firstVisibleLine < linesBuffer.size() - _maxVisibleLines) {
        firstVisibleLine++;
        _redraw = true;
    }
}

void ScrollableTextArea::scrollToLine(size_t lineNumber) {
    const size_t maximum = linesBuffer.size() > _maxVisibleLines ? linesBuffer.size() - _maxVisibleLines : 0;
    firstVisibleLine = lineNumber > maximum ? maximum : lineNumber;
    _redraw = true;
}

String ScrollableTextArea::getLine(size_t lineNumber) {
    return lineNumber < linesBuffer.size() ? linesBuffer[lineNumber] : String("");
}

size_t ScrollableTextArea::getMaxLines() { return linesBuffer.size(); }

void ScrollableTextArea::show(bool force) {
    draw(force);

    while (check(SelPress)) {
        if (check(EscPress)) return;
        update(force);
        yield();
    }
    while (!check(SelPress)) {
        if (check(EscPress)) return;
        update(force);
        yield();
    }
}

uint32_t ScrollableTextArea::getMaxVisibleTextLength() { return _maxVisibleLines * _maxCharactersPerLine; }

void ScrollableTextArea::update(bool force) {
#ifdef HAS_ENCODER
    int32_t rotarySteps = drainRotarySteps();
    if (rotarySteps != 0) {
        check(PrevPress);
        check(NextPress);
        check(UpPress);
        check(DownPress);
        const size_t maximum = linesBuffer.size() > _maxVisibleLines ? linesBuffer.size() - _maxVisibleLines : 0;
        const int64_t target = int64_t(firstVisibleLine) - int64_t(rotarySteps);
        scrollToLine(target < 0 ? 0 : uint64_t(target) > maximum ? maximum : size_t(target));
        vTaskDelay(4 / portTICK_PERIOD_MS);
        PrevPress = false;
        NextPress = false;
        UpPress = false;
        DownPress = false;
    }
#endif

    if (check(PrevPress) || check(UpPress)) scrollUp();
    else if (check(NextPress) || check(DownPress)) scrollDown();

    draw(force);
}

void ScrollableTextArea::fromFile(File file) {
    clear();
    while (file.available()) addLine(file.readStringUntil('\n'));

    draw(true);
    delay(100);
    draw(true);
}

void ScrollableTextArea::clear() {
    firstVisibleLine = 0;
    lastVisibleLine = 0;
    linesBuffer.clear();
    _redraw = true;
}

void ScrollableTextArea::fromString(const String &text) {
    clear();
    int startIdx = 0;
    int endIdx = 0;

    while (endIdx < text.length()) {
        if (text[endIdx] == '\n') {
            addLine(text.substring(startIdx, endIdx));
            startIdx = endIdx + 1;
        }

        endIdx++;
    }

    // Add the last line if there's remaining text (text does not ends with \n)
    if (startIdx < text.length()) { addLine(text.substring(startIdx, endIdx)); }
}

// for devices it will act as a scrollable text area
void ScrollableTextArea::addLine(const String &text) {
    if (text.isEmpty()) {
        linesBuffer.emplace_back("");
        _redraw = true;
        return;
    }

#ifdef FLUX_DELTA_BUILD
    if (_deltaStandard) {
        size_t start = 0;
        while (start < text.length()) {
            size_t end = start;
            const char *cursor = text.c_str() + start;
            while (*cursor) {
                const char *next = cursor;
                delta::ui::nextGlyph(next);
                const size_t candidate = next - text.c_str();
                if (end > start && delta::ui::measure(text.substring(start, candidate).c_str(), 10000) > _width - 6) break;
                end = candidate;
                cursor = next;
            }
            String line = text.substring(start, end);
            if (line.endsWith("\r")) line.remove(line.length() - 1);
            linesBuffer.emplace_back(line);
            start = end;
        }
        _redraw = true;
        return;
    }
#endif

    String buff;
    size_t start = 0;
    bool firstLine = true;

    // Automatically split into multiple lines
    while (start < text.length()) {
        size_t len = _maxCharactersPerLine;

        if (!firstLine && _indentWrappedLines && len > 1) {
            buff = " " + text.substring(start, start + len - 1); // Reduce length for space
            start += len - 1;
        } else {
            buff = text.substring(start, start + len);
            start += len;
        }
        if (buff.endsWith("\r")) buff.remove(buff.length() - 1);

        linesBuffer.emplace_back(buff);
        firstLine = false;
    }

    _redraw = true;
}

void ScrollableTextArea::draw(bool force) {
    if (!_redraw && !force) return;
    const size_t maximum = linesBuffer.size() > _maxVisibleLines ? linesBuffer.size() - _maxVisibleLines : 0;
    if (firstVisibleLine > maximum) firstVisibleLine = maximum;

#ifdef FLUX_DELTA_BUILD
    if (_deltaStandard) {
        uint16_t *canvas = deltaUiCanvas(true, false);
        if (canvas) {
            const auto &appearance = deltaUiAppearance();
            const auto colors = delta::ui::palette(appearance.theme);
            delta::ui::header(canvas, _title.c_str(), appearance, &deltaUiStatus());
            size_t index = firstVisibleLine;
            for (size_t row = 0; row < _maxVisibleLines && index < linesBuffer.size(); ++row, ++index)
                delta::ui::textClip(canvas, linesBuffer[index].c_str(), _startX,
                                    _startY + row * _pixelsPerLine, _width - 6, colors.text);
            if (maximum) {
                const int thumb = _height * _maxVisibleLines / linesBuffer.size();
                delta::ui::box(canvas, 312, _startY, 2, _height, colors.line, 1);
                delta::ui::box(canvas, 312, _startY + (_height - thumb) * firstVisibleLine / maximum,
                                2, thumb < 4 ? 4 : thumb, colors.accent, 1);
            }
            lastVisibleLine = index;
            deltaUiPresent();
            _redraw = false;
            return;
        }
    }
#endif

    _scrollBuffer.fillRect(_startX, _startY, _width, _height, fluxConfig.bgColor);
    _scrollBuffer.setTextColor(fluxConfig.priColor);
    uint8_t _fSize = tft.getTextSize();
    tft.setTextSize(_fontSize);

    uint16_t yOffset = 0;
    size_t lines = 0;

    int32_t tmpHeight = _height;

    size_t idx{firstVisibleLine};
    while (yOffset < tmpHeight && lines < _maxVisibleLines && idx < linesBuffer.size()) {
        _scrollBuffer.drawString(linesBuffer[idx], 0 + _startX, _startY + yOffset);
        yOffset += _pixelsPerLine;
        lines++;
        idx++;
    }

    lastVisibleLine = firstVisibleLine + lines;
    tft.setTextSize(_fSize);

    _redraw = false;
}
