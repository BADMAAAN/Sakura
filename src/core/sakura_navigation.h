#pragma once
#include <stdint.h>

enum class SakuraActionKind : uint8_t { Section = 1, Screen = 2, Ssh = 3, Qr = 4 };
bool sakuraIsPinned(SakuraActionKind kind, uint32_t id);
bool sakuraSetPinned(SakuraActionKind kind, uint32_t id, bool pinned);
void sakuraForget(SakuraActionKind kind, uint32_t id);
void sakuraFavoritesMenu();
void sakuraEditFavoritesMenu();
void sakuraRecentMenu();
// Records only allowlisted section/screen identifiers, never profile or QR contents.
void sakuraRecordMenu(const char *menu, const char *label);
