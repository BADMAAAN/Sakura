#pragma once
#include <stdint.h>

enum class InputSource : uint8_t { Back, Select, Encoder };
enum class InputKind : uint8_t { Press, Release, Long, Left, Right };
void inputDiagnosticEvent(InputSource source, InputKind kind, uint32_t recognitionUs);
void inputDiagnosticConsumed(bool back);
void inputDiagnosticScreen(const char *screen);
void inputDiagnosticAction(const char *action, const char *result);
void inputDiagnosticsMenu();
void inputButtonsScreen();
