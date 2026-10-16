#pragma once
#ifdef FLUX_DELTA_BUILD
#include <Arduino.h>
#include <vector>
#include <delta/widgets.h>

struct Option;
// One reusable PSRAM canvas. Existing handlers retain input, data and action ownership.
uint16_t *deltaUiCanvas(bool clear = true, bool decoration = true, bool retained = false);
void deltaUiPresent(int x = 0, int y = 0, int width = 320, int height = 170);
const delta::Appearance &deltaUiAppearance();
const delta::Status &deltaUiStatus();
bool deltaUiMenu(const char *title, const std::vector<Option> &items, int selected);
void deltaUiMenuTick();
void deltaUiMenuEnd();
void deltaUiInvalidate();
bool deltaUiRestore();
void deltaUiRequestBenchmark();
void deltaUiPhysicalInput(uint32_t sampledAtUs);
void deltaUiInputConsumed();
uint32_t deltaUiMapColor(uint32_t color);

bool deltaUiSelect(const String &name, int direction);
void deltaUiEnter();
void deltaUiTick(bool ambient = false);
void deltaAppearanceMenu();
void deltaFavoritesMenu();
uint32_t deltaUiFavoriteSections();
#endif
