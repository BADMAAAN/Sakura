# FLUX DELTA — карта общего интерфейса

Итог реализации и границы проверки: [системный UI](FLUX-DELTA-SYSTEM-UI.md), [аппаратная приёмка](FLUX-DELTA-SYSTEM-UI-VALIDATION.md). Таблица ниже сохраняет исходный аудит; Legacy в ней описывает baseline.

Аудит исходной рабочей версии `6ade36dda06bf667bf0084cc403ddb3a7b36af3d` от 15 октября 2026. Это карта физических путей выполнения, а не перечень экранов предпросмотра. Модульные специализированные экраны и их прямой TFT-вывод перечисляются в отдельном инвентаре модулей. Статусы ниже фиксируют исходное состояние до переноса; прохождение проверки нельзя выводить из наличия кода.

## Физический путь

`MainMenu::begin()` → `loopOptions(..., MENU_TYPE_MAIN, ..., _currentIndex)` → `Option::hover` → `MainMenu::renderFluxMainMenu` → `deltaUiSelect` / `deltaUiTick` → общий C++ DELTA renderer → `tft.pushImage` → ST7789.

Выбор вызывает исходную `Option::operation()`, зарегистрированную как `_menuItems[i]->optionsMenu()`. Каждый `*Menu::optionsMenu()` затем строит свои реальные `Option` и вызывает **тот же** `loopOptions`, обычно с `MENU_TYPE_SUBMENU`. Вложенные настройки часто используют `MENU_TYPE_REGULAR`. Именно два последних режима пока проходят через старые `drawSubmenu` и `drawOptions`. `deltaUiTick` в исходной версии вызывается только для MAIN.

Энкодер обрабатывается в `loopOptions` через `drainRotarySteps()`; для T-Embed знак меняется. Обрабатывается весь накопленный пакет, пропускаются disabled options, сохраняется итоговый index. Back возвращает -1. Индекс главной категории сохраняется в `_currentIndex`. У вложенных функций сохранение фокуса зависит от переданного индекса: Delta Appearance передаёт текущий index, но большинство ConfigMenu-подменю пересоздаются с 0.

## Общие экраны и границы переноса

| Экран / семейство | Место вызова | Фактический рендерер | Управление | Оформление в исходной версии | Требуемая адаптация | Статус переноса при аудите | Проверка после переноса |
|---|---|---|---|---|---|---|---|
| Home, 12 доступных категорий + Favorites | `src/core/main_menu.cpp:256 MainMenu::begin` | `delta_ui.cpp:deltaUiTick` → `include/delta/core.h:render` | wheel, Select, Back/wake | Dark Sakura, один значок | Общая анимация входа/возврата, без изменения категорий | Уже DELTA; физически подтверждён в предыдущей установке | Быстрый wheel, enter/back, прежняя категория, sleep/wake |
| Меню WiFi/Bluetooth/RF/RFID/IR/Files/Scripts/LoRa/Ethernet/Others | `src/core/menu_items/*::optionsMenu` | `loopOptions SUBMENU` → `drawSubmenu` | wheel, Select, Back | Заголовок + один крупный пункт, старые bitmap glyphs | Компактный список DELTA с несколько видимыми строками | Legacy | Все реальные option callbacks доступны; deepest Back |
| Обычные параметры, переключатели, выбор чисел/модуля | `src/core/settings.cpp`, `ConfigMenu.cpp`, `wifi_common.cpp` | `loopOptions REGULAR` → `drawOptions` → `displayScrollingText` | wheel, Select, Back, serial remote force | `>` и прямоугольное инверсное выделение | Общий focus, label/value, выбранное значение, clipping | Legacy | Disabled, selected, empty, long values, wraparound |
| Settings / Config | `ConfigMenu::optionsMenu` | SUBMENU | wheel, Select, Back | Legacy submenu | Общий DELTA list | Legacy | Открыть Appearance, Display, System, About и вернуться |
| Delta Appearance | `delta_ui.cpp:deltaAppearanceMenu` | SUBMENU; цикл с текущим index | wheel, Select циклически меняет значение, Back | Старый экран поверх новых настроек | Общий список, значения отдельно, On/Off switch, immediate theme refresh | Legacy | Все 12 строк; смена темы не сбрасывает focus |
| Favorites / Edit Favorites | `deltaFavoritesMenu`, `editFavorites` | SUBMENU | wheel, Select, Back | Legacy | Общий список, checked marker | Legacy | Сохранить выбор и открыть настоящий handler |
| Display & UI | `ConfigMenu::displayUIMenu` | SUBMENU + REGULAR | wheel, Select, Back | Legacy | Единый DELTA list и selectors | Legacy | Brightness, dim, orientation, UI color/theme |
| Customization | `ConfigMenu::customizationMenu` | SUBMENU + REGULAR | wheel, Select, Back | Legacy; часть параметров относится к старому Home | DELTA list; сохранить доступ к существующим параметрам | Legacy | Не терять настройки/LED handlers |
| LED / Audio | `ConfigMenu::ledMenu`, `audioMenu` | SUBMENU + `settings.cpp` selectors | wheel, Select, Back | Legacy | DELTA list, numeric/value rendering | Legacy | Проверить UI без изменения аппаратных protection settings |
| System | `ConfigMenu::systemMenu` | SUBMENU | wheel, Select, Back | Legacy | DELTA list | Legacy | Startup App, Hide/Show, Clock, Keyboard, Advanced |
| Advanced / Pins / Dev Mode | `advancedMenu`, `pinsMenu`, `devMenu` | SUBMENU, REGULAR, keyboard | wheel, Select, Back | Legacy | DELTA list, shared input/dialogs | Legacy | Навигация и cancel; не выполнять factory reset |
| Power | `ConfigMenu::powerMenu` | SUBMENU + confirmation | wheel, Select, Back | Legacy | DELTA list + dialog | Legacy | UI только; не запускать Deep Sleep автоматически |
| Клавиатура QWERTY/AZERTY/QWERTZ | `keyboard()` → `generalKeyboard<...>` в `mykeyboard.cpp:449` | Прямые `tft.drawString/fillRect/drawRect` | wheel/keys/touch, caps/delete/space/OK/Exit | Клетки, моноширинные bitmap glyphs, инверсия | Общий DELTA keyboard renderer; оставить весь input/edit алгоритм | Legacy | Mask password, max length, delete/caps/space, Esc sentinel |
| Числовая / hex клавиатура | `num_keyboard`, `hex_keyboard` | Тот же `generalKeyboard` с другой keyset | Те же обработчики | Legacy | Один общий путь с основной клавиатурой | Legacy | Частота, IP, key fields, cancel без записи |
| Язык клавиатуры | `setKeyboardLanguage` | SUBMENU | wheel, Select, Back | Legacy | Общий list | Legacy | Три раскладки не потеряны |
| Браузер SD/LittleFS и file picker | `sd_functions.cpp:601 loopSD` → `listFiles` | `display.cpp:1259 listFiles`, затем `displayScrollingText` | wheel, short/long Select, Back, page | `>` и цвет текста, старый внешний border | DELTA file/folder icons, focus, FS/path context, clipping | Legacy | Empty folder, back root, long path, SD missing, picker filter |
| File context actions | `loopSD` строки 779/795/968 | REGULAR `loopOptions` | wheel, Select, Back | Legacy | Общий DELTA list | Legacy | Сохранить callbacks rename/copy/paste/delete/open; не исполнять разрушительное в QA |
| File Info | `sd_functions.cpp:1046 fileInfo` | `drawMainBorderWithTitle`, `padprint*`, direct TFT | key to exit | Старый title/text | Общая page/header/content typography | Legacy | Длинное имя, FS, размер/hash без overflow |
| View File | `sd_functions.cpp:999 viewFile` | `ScrollableTextArea("VIEW FILE")` | wheel up/down, Select exit | Старый текст, `...` continuation | DELTA header + readable data area, без лепестков | Legacy | Много строк, пустой файл, scroll границы |
| Device Info | `utils.cpp:169 showDeviceInfo` | `ScrollableTextArea("DEVICE INFO")` | wheel, Select exit | Старый текстовый экран | Общая page/text area | Legacy | Видны реальные сведения; длинные строки не выходят за экран |
| Прочие scrollable result areas | `ScrollableTextArea` explicit rectangle constructor | `_scrollBuffer` **является `tft`**, не sprite | `update`: drainRotarySteps + Prev/Next | Legacy bitmap text | Сохранить размеры данных, общая палитра/текст; no decoration in data | Legacy | Font metrics, short buffer, top/bottom, low memory |
| Ошибка / warning / info / success / text notification | Весь core + modules → `displayError/Warning/Info/Success/TextLine` | `displayRedStripe` + `wrapText` | optionally AnyKey; legacy wait duration | Яркая центральная цветная полоса | Общий DELTA card с семантическим accent и корректным wrap | Legacy | Empty/long/no-space/newline; bounded box; sleep gate |
| Confirm / choice dialog | `ConfigMenu::advancedMenu`, `powerMenu`; другие callers | `displayMessage` → `drawButton` | Prev/Next, Esc сейчас перемещает, Select | Отдельные рамки/чёрный фон | Единый DELTA dialog, ясный cancel, реальные returned indices | Legacy | 1/2/3 buttons, zero guard, cancel returns safe index |
| Progress | File sharing `sendFile/recvFile`, module callbacks → `progressHandler` | Прямой fillRect/drawRect + notification | Handler owns operation/cancel | Старый прямоугольник и цветная полоса | DELTA bounded progress, message, indeterminate for total0 | Legacy | total0, negative/over-total, redraw once/value, no changed operation |
| Wi-Fi connect/scanning/empty/error | `wifi_common.cpp:_connectToWifiNetwork`, `wifiConnectMenu` | notification, REGULAR options, `drawMainBorderWithTitle/padprint` | wheel/Select/Back, password keyboard | Legacy; SSID/RSSI/security/channel packed into one label | Shared DELTA list with structured display rows; preserve WiFi APIs/closures | Legacy | Passive scan only if requested; static real-data fixtures for layout; no invented scans |
| Wireguard / WebUI / file sharing / Mass Storage / I2C finder | core helper pages in `wifi`, `connect`, `massStorage.cpp`, `i2c_finder.cpp` | Common border/title + direct padprint/TFT body | Existing handler-specific buttons | Mixed legacy | Shared page/header/text/notification; dedicated body only where necessary | Legacy | Offline/no-SD/error paths, no automatic network/USB action |
| Clock and date/time setup | `settings.cpp:setClock`, `runClockLoop`; `ClockMenu` | selectors + direct TFT clock | wheel/Select/Back | Legacy specialized clock / numeric selectors | Common selectors + readable clock typography | Legacy | Manual set selectors; leave clock/RTC algorithms intact |
| Boot/splash | `main.cpp` → `flux_branding.cpp` or DELTA setup guard | image/sprite/direct TFT | Startup | DELTA guard skips old slideshow | Do not reintroduce old photo/splash after wake | Existing guard | Real boot log + visible Home |
| Image/GIF/PNG/JPEG/media display | `drawImg/showGif/showJpeg/drawBmp/drawPNG` in `display.cpp` | Decoder → `pushImage` / pixels | Caller controls | Source image pixels | Preserve pixels; skin file selection, errors, surrounding chrome | Intentionally content-owned | Missing file, failed allocation, no palette remap of image |

## Integration contract

1. Extend existing `delta_ui` plus a small portable `include/delta/...` renderer. Reuse Home's font atlas, palettes and buffer where practical. Do not create a new menu/state/callback registry.
2. In DELTA builds route both `drawOptions` and `drawSubmenu` to the same compact list renderer. All existing callers immediately gain the skin. Preserve `Option.operation`, hover callbacks, enabled/selected/hovered semantics, forced serial selection and interpreter checks.
3. Keep `loopOptions` in charge of input. A timestamp transition stores one current target; each wheel packet replaces it. No per-event queued transitions. Render transition frames from the existing outer loop, never an inner waiting loop. The existing legacy 64ms submenu animation must be skipped under DELTA.
4. Return to Home through the existing `MainMenu::begin/_currentIndex` path. Drain the entering Select/Back once without delaying the animation. Preserve caller-local index where it already exists; standard settings rebuilding with zero need a retained index.
5. Common pages should reserve a compact header and leave the existing working content area as large as possible. Graphs and terminal output get static background, no petals. Data and labels must be drawn with measured widths and explicit clips/ellipsis; don't assume `LW*fontSize` after switching to proportional text.
6. `listFiles` receives only index/list. FS and current Folder are owned by `loopSD`, so add a tiny context handoff or optional arguments at its actual call site. Merely styling `listFiles` cannot display an honest SD/LittleFS/path indicator.
7. Keyboard drawing is one common templated block: replace only rendering calls with a shared skin, retain key arrays/cursor/input/cancel/max-size/password logic. Keep printable password text masked in every redraw. Do not print it into diagnostics.
8. `ScrollableTextArea::setup` derives columns/lines from active font metrics; recompute after changing font. Its backing buffer is a vector of text, not a framebuffer. Avoid creating one sprite per line or character.
9. One persistent RGB565 frame at 320×170 costs 108800 bytes in PSRAM. Reuse or a bounded viewport; on allocation failure draw the same DELTA information directly with reduced effects, do not silently lose controls or revert the entire standard UI to Bruce.
10. Gate every animation tick on `isScreenOff || isSleeping`. Actual decoder `pushImage`, inherited `drawPixel/write` and `native()` may bypass logger sleep state; prevent animation at the owning page, rather than relying solely on logger interception.
11. `FluxTheme` in `theme.cpp` loads user JSON/image themes. Its colors are separate from `delta::Appearance` palette and are not automatically changed by selecting Sakura Dawn. A common active-palette accessor is needed so all newly skinned components immediately agree without rewriting persisted user settings.
12. Keep image content, waveform/spectrum sample colors, protocol bits and QR data untouched. Global remapping of TFT_BLACK/TFT_WHITE or all RGB565 values would corrupt data visualization and does not solve layout/type hierarchy.

## TFT/HAL boundaries and bypasses

- `include/globals.h:43` declares global `tft_logger tft`, plus `tft_sprite sprite/draw`.
- `include/tftLogger.h:56`: `tft_logger` derives from `tft_display`; logs/redraws common fill/line/rect/shape/string/print calls. Existing `setRenderTarget` redirects those overrides to an offscreen `TFT_eSprite`.
- `lib/HAL/display/tft.h` selects the physical driver. T-Embed uses `USE_TFT_ESPI`; `tft_display` privately inherits `TFT_eSPI` and exposes the specific methods in `tftespi.h`.
- Inherited `drawPixel`, `pushImage`, `drawXBitmap`, gradients, write and calls through `native()` are outside most logger overrides. `tft_sprite::pushSprite` also transfers directly. A logger skin alone therefore cannot cover all screens.
- `displayScrollingText` and `drawSubmenuBuffered` allocate their own persistent sprites with fallback. Ensure new DELTA paths do not accidentally allocate these legacy sprites as well as the main buffer.
- `flux_branding.cpp` and legacy MainMenu sprites use native display APIs; the DELTA guard already prevents the legacy main renderer from being the target path.
- `display.cpp` decoders send actual image rows with `pushImage`; these are content passthroughs rather than missing UI migrations.
- Complete core call-site evidence is `.codex/ui-system/common-screen-inventory.json`. It records every direct display expression and common UI call in core at the audited commit, including source line and nearby containing signature. Module inventory is intentionally owned separately.

## Baseline edge cases to cover during integration

- `wrapText` advances `splitPos+1` even for forced split without whitespace: drops a character from long unbroken strings.
- `progressHandler` maps with total0: division by zero risk; width also needs clamp.
- `displayMessage` uses modulo totalButtons: zero-button input must not enter that loop. Esc currently changes selection; safe cancellation semantics need explicit handling while preserving No/Yes returned values.
- `ScrollableTextArea::getLine(size)` returns `linesBuffer[size]` for out-of-range; drawing must never use it for an empty buffer. Related `scrollToLine` subtraction uses size_t and must be bounded for short buffers.
- REGULAR scrolling uses a static scroll offset/timestamp shared by all callers. Reset/clip on focus and page changes; otherwise a newly selected long label begins at an old offset.
- `loopSD` assumes a synthetic back entry in `fileList`; test missing/unmounted FS before indexing.
- Home's current shared glyph atlas accepts ASCII32..126 and substitutes `?` outside it. New common text cannot promise full Unicode support without actual font coverage. SSIDs/filenames may contain UTF-8; clipping must not cut a multibyte sequence. Document remaining glyph coverage honestly.

## Minimum physical / regression evidence

Run Home → each existing category → nested selector → Back; retain real operation pointers. Exercise Appearance theme change, ordinary toggle/value picker, long list, long strings, disabled/empty, dialog cancel, keyboard masked input/Exit, Files SD/LittleFS/no SD, Device Info scroll, RF parameter chooser and SSH connection fields without starting radio/network actions. Measure render/transfer and real heap/free-PSRAM before/after repeated open/close. Run existing battery/power/LittleFS tests and verify the installed app-only binary size remains below 0x470000. Record physical appearance separately from static-render captures and successful build/flash.
