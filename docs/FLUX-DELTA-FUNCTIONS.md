# FLUX DELTA: function map

Source inspection for the `flux-delta` target, based on
`flux-t-embed-cc1101`. This is an inventory of available code paths, not a claim
that every peripheral or feature has been tested on hardware.

## Main navigation

The baseline target registers **12 categories**. DELTA appends **Favorites**,
making **13 entries before user filtering**. Categories listed in
`fluxConfig.disabledMenus` are omitted; Favorites also respects that filter.
The number 13 is a result of this implementation, not an assumed count of
features or working peripherals.

| Baseline order | Category | Function groups | Original entry point |
|---|---|---|---|
| 1 | WiFi | Connection/AP management, network tools, SSH/Telnet/TCP clients, analysis, WireGuard, WebUI-related settings | `src/core/menu_items/WifiMenu.cpp`: `WifiMenu::optionsMenu()` |
| 2 | BLE | Scanning, media/keyboard/presenter tools, beacon and BLE tools | `src/core/menu_items/BleMenu.cpp`: `BleMenu::optionsMenu()` |
| 3 | RF | Scan/copy, raw files, spectrum/waterfall, radio configuration and existing RF tools | `src/core/menu_items/RFMenu.cpp`: `RFMenu::optionsMenu()` |
| 4 | LoRa | Chat, username and frequency settings | `src/core/menu_items/LoRaMenu.cpp`: `LoRaMenu::optionsMenu()` |
| 5 | IR | Existing IR library, custom files, receive tools and pin/repeat settings | `src/core/menu_items/IRMenu.cpp`: `IRMenu::optionsMenu()` |
| 6 | Ethernet | Existing W5500 host/network tools | `src/core/menu_items/EthernetMenu.cpp`: `EthernetMenu::optionsMenu()` |
| 7 | RFID | Tag/file operations, NDEF, configured reader interfaces and supported extensions | `src/core/menu_items/RFIDMenu.cpp`: `RFIDMenu::optionsMenu()` |
| 8 | Files | SD/LittleFS browsing, WebUI, Connect and USB mass storage | `src/core/menu_items/FileMenu.cpp`: `FileMenu::optionsMenu()` |
| 9 | JS Interpreter | JS/BJS script browser, loader and interpreter return path | `src/core/menu_items/ScriptsMenu.cpp`: `ScriptsMenu::optionsMenu()` |
| 10 | Clock | Existing clock screen and timer | `src/core/menu_items/ClockMenu.cpp`: `ClockMenu::optionsMenu()` |
| 11 | Others | QR codes, microphone tools, USB/HID, iButton and existing utilities | `src/core/menu_items/OthersMenu.cpp`: `OthersMenu::optionsMenu()` |
| 12 | Config | Display/UI, customization, LEDs/audio, system, power, app store and About | `src/core/menu_items/ConfigMenu.cpp`: `ConfigMenu::optionsMenu()` |
| Added | Favorites | Saved shortcuts to enabled baseline categories; Add/Remove editor | `src/core/delta_ui.cpp`: `deltaFavoritesMenu()` |

LoRa, Ethernet and optional RFID interfaces can require external hardware.
State-dependent items remain conditional, including WiFi connection actions,
BLE disconnect, SD browsing, NDEF emulation and reader-specific tools.

## Preserved paths

- **SSH Client:** `WiFi → SSH` invokes the original
  `lambdaHelper(ssh_setup, String(""))` callback in `WifiMenu.cpp`.
- **Files:** SD appears after successful `setupSdCard()`; LittleFS uses
  `loopSD(LittleFS)`. WebUI and, on supported USB hardware, Mass Storage retain
  their original callbacks.
- **Connect:** `Files → Connect` exposes Send File, Recv File, Send Cmds and
  Recv Cmds. `ConnectMenu` exists in the source but is not registered as a
  baseline top-level category; DELTA does not invent a duplicate entry.
- **Scripts:** `src/modules/bjs_interpreter/interpreter.cpp` searches `/scripts`,
  `/FluxScripts` and `/FluxJS`, checking SD before LittleFS for each candidate.
  The existing browser handles directories and JS/BJS files; `Load...` retains
  `run_bjs_script()` and its filesystem selector.
- **DELTA controls:** `Config → Delta Appearance` opens theme, quality, petals,
  transitions, glitch, Reduced Motion, status/icon visibility, brightness,
  Favorites and Ambient Clock controls. Existing Config groups remain present.

## Compile-time and callback boundaries

`boards/lilygo-t-embed-cc1101/flux-t-embed-cc1101.ini` already defines
`FLUX_HIDE_NRF24`, `FLUX_HIDE_FM` and `FLUX_HIDE_GPS`. Their top-level entries are
hidden for this target; the corresponding source features have not been
deleted. Other profiles can expose a different list. `LITE_VERSION`,
`DISABLE_INTERPRETER` and peripheral-specific macros also affect availability.

`MainMenu::begin()` in `src/core/main_menu.cpp` retains the original
`_menuItems[i]->optionsMenu()` operations and applies `disabledMenus` before
building the visible options. Favorites invokes the same category entry
points, rather than copying their implementations.

`loopOptions()` in `src/core/display.cpp` still owns enabled-item selection,
encoder steps, Select/Back, remote selection and callback invocation. The
visual shell changes rendering. `MainMenu::_currentIndex` retains the selected
icon when returning from a category during the current boot; this is not a
claim of selection persistence across reboot.
