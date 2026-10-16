# Remaining warning inventory

Source: the successful FLUX and Bruce reliability builds recorded in
`ai/context/reliability-flux-build.log` and `ai/context/reliability-bruce-regression.log`.

## Safe project-source fixes

- `FileMenu.cpp`: replace deprecated implicit `this` capture with explicit capture.
- `reverseShell.cpp`: explicitly accept WebSocket PING/PONG events already handled by the library.
- `ST25R3916.cpp`: remove an always-true `uint8_t < 256` guard without changing the executed branch.
- Unused project-local state requires separate control-flow inspection before removal.

## Deferred behavior changes

- `karma_attack.cpp`: two `180000` to `uint16_t` overflows are real, but changing the type changes attack duration and remains outside safe triage.
- Volatile counters in Wi-Fi analyzers are ISR/concurrency-sensitive and are not changed merely to silence C++20 diagnostics.
- RFID empty-body and protocol warnings require device/protocol validation before editing.

## Dependency/toolchain warnings

Narrowing/deprecation warnings in ESP Chameleon Ultra, IRremoteESP8266, FastLED,
NimBLE, OneWire, PNGdec and ESP8266Audio belong to pinned dependencies. TFT_eSPI/HAL
LTO ODR diagnostics arise from the existing multi-display abstraction. These are
tracked but not patched in vendored/generated build trees during this task.

This triage intentionally makes no radio timing, packet, credential, attack-module,
bootloader, partition, or hardware pin behavior changes.
