#include "input_diagnostics.h"
#ifdef SAKURA_BUILD
#include "delta_ui.h"
#include "display.h"
#include <globals.h>

#ifndef SAKURA_INPUT_TRACE
#define SAKURA_INPUT_TRACE 0
#endif

namespace {
struct Record {
    uint32_t time = 0, count = 0, recognitionUs = 0;
    InputSource source = InputSource::Back;
    InputKind kind = InputKind::Release;
    const char *screen = "menu", *action = "none", *result = "observed";
    const char *logical = "None", *gesture = "None";
};
portMUX_TYPE diagnosticMux = portMUX_INITIALIZER_UNLOCKED;
Record last;
bool active = false;
#if SAKURA_INPUT_TRACE
Record trace[16];
unsigned traceCount = 0;
#endif
void remember() {
#if SAKURA_INPUT_TRACE
    trace[traceCount++ % 16] = last;
#endif
}
const char *sourceName(InputSource source) {
    return source == InputSource::Back ? "Top / GPIO6" : source == InputSource::Select ? "Encoder / GPIO0" : "Encoder rotation";
}
const char *kindName(InputKind kind) {
    switch (kind) {
    case InputKind::Press: return "Short press";
    case InputKind::Release: return "Release";
    case InputKind::Long: return "Long hold";
    case InputKind::Left: return "Rotate left";
    case InputKind::Right: return "Rotate right";
    }
    return "None";
}
void buttonsScreen() {
    portENTER_CRITICAL(&diagnosticMux);
    last = Record(); last.screen = "diagnostics.buttons"; active = true;
    portEXIT_CRITICAL(&diagnosticMux);
    uint32_t drawn = UINT32_MAX;
    for (;;) {
        if (check(EscPress)) break;
        check(SelPress); check(PrevPress); check(NextPress);
        drainRotarySteps();
        portENTER_CRITICAL(&diagnosticMux);
        const Record snapshot = last;
        portEXIT_CRITICAL(&diagnosticMux);
        if (snapshot.count != drawn) {
            char events[24], timing[32], gesture[48];
            snprintf(events, sizeof(events), "%lu", (unsigned long)snapshot.count);
            snprintf(timing, sizeof(timing), "%lu us", (unsigned long)snapshot.recognitionUs);
            snprintf(gesture, sizeof(gesture), "%s / %s", snapshot.gesture, kindName(snapshot.kind));
            if (auto *frame = deltaUiCanvas()) {
                const delta::ui::MenuRow rows[] = {
                    {"Source", snapshot.count ? sourceName(snapshot.source) : "Waiting"},
                    {"Logical event", snapshot.logical}, {"Gesture", gesture}, {"Events", events}, {"Recognized", timing}
                };
                delta::ui::list(frame, deltaUiAppearance(), "Buttons", rows, 5, 4, &deltaUiStatus());
                deltaUiPresent();
            }
            drawn = snapshot.count;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    portENTER_CRITICAL(&diagnosticMux);
    active = false; last.screen = "menu";
    portEXIT_CRITICAL(&diagnosticMux);
#if SAKURA_INPUT_TRACE
    // Dump a bounded ring only after leaving the screen; input sampling never prints.
    const unsigned count = traceCount < 16 ? traceCount : 16;
    for (unsigned i = 0; i < count; ++i) {
        const Record &r = trace[(traceCount - count + i) % 16];
        Serial.printf("[INPUT TRACE] t=%lu source=%u type=%u screen=%s action=%s result=%s\n",
                      (unsigned long)r.time, unsigned(r.source), unsigned(r.kind), r.screen, r.action, r.result);
    }
#endif
}
}

void inputDiagnosticEvent(InputSource source, InputKind kind, uint32_t recognitionUs) {
    portENTER_CRITICAL(&diagnosticMux);
    if (active || SAKURA_INPUT_TRACE) {
        last.time = micros(); ++last.count; last.source = source; last.kind = kind;
        last.recognitionUs = recognitionUs; last.action = "none"; last.result = "recognized";
        if (kind != InputKind::Release) {
            last.logical = source == InputSource::Back ? "Back/Cancel" : source == InputSource::Select ? "Select" : "Scroll";
            last.gesture = kind == InputKind::Long ? "Long" : source == InputSource::Encoder ? "Rotation" : "Short";
        }
        remember();
    }
    portEXIT_CRITICAL(&diagnosticMux);
}
void inputDiagnosticConsumed(bool back) { inputDiagnosticAction(back ? "back/cancel" : "select", "consumed"); }
void inputDiagnosticScreen(const char *screen) {
    portENTER_CRITICAL(&diagnosticMux); last.screen = screen; portEXIT_CRITICAL(&diagnosticMux);
}
void inputDiagnosticAction(const char *action, const char *result) {
    portENTER_CRITICAL(&diagnosticMux);
    if (active || SAKURA_INPUT_TRACE) { last.time = micros(); last.action = action; last.result = result; remember(); }
    portEXIT_CRITICAL(&diagnosticMux);
}
void inputDiagnosticsMenu() {
    std::vector<Option> choices = {{"Buttons", buttonsScreen}, {"Back", []() {}}};
    while (loopOptions(choices, MENU_TYPE_SUBMENU, "Diagnostics") == 0) {}
}
void inputButtonsScreen() { buttonsScreen(); }
#endif
