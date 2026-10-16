# FLUX DELTA local rebuild

Latest Cyber Sakura refinement: [performance and device validation](FLUX-DELTA-CYBER-VALIDATION.md),
[native before/after gallery](FLUX-DELTA-CYBER-VISUAL.md).
Previous interior refinement: [validation and device results](FLUX-DELTA-INTERIOR-VALIDATION.md),
[six-screen before/after comparison](FLUX-DELTA-INTERIOR-VISUAL.md).
Earlier sections and system-preview captures describe their original revisions.

This implementation was rebuilt locally from FLUX baseline commit
`6345ffad1202fe8e28e75cfedbdde51353fa98e5`. It does not claim to recover the
unavailable cloud revision. The original checkout at `C:\Projects\FLUX` is
preserved. Development branch: `flux-delta-rebuild`.

## Interface

Dark Sakura uses a deterministic, indexed 320×170 illustration: a branching ink
tree, layered blossom clusters and an open navigation slot on the right. The
same generated artwork and C++ renderer run on ESP32 and in the Windows preview.
Three palettes provide Dark Sakura, Sakura Dawn and Monochrome. Thirteen drawn
icons cover the twelve existing target categories and the added Favorites entry.
See [the function map](FLUX-DELTA-FUNCTIONS.md).

Only one category icon and label are visible. A time-based eased transition
fades the outgoing item before introducing the incoming item, with a small
directional offset. New encoder events update the selection immediately.
Transitions are selectable at 140/160/180 ms. Petals have four density levels;
Reduced Motion disables decorative movement and transitions. Subtle Glitch is
optional. Status and icons can be hidden. Ambient Clock is an awake screen.

Settings → Delta Appearance exposes these controls, brightness and Favorites.
The compact versioned appearance record is stored in its own NVS namespace only
after an explicit setting change. Unknown/corrupt records use defaults; v1 records
migrate in memory. Existing FLUX configuration and handlers remain in use.

If an existing startup app is saved, DELTA offers Main Menu by default and an
explicit Run action before dispatching it. The saved selection is preserved.
Highlighting Run or leaving the prompt does not launch it. This also prevents
automatic RF/script execution during the initial device checks.

## Reliability and power

LittleFS starts with `begin(false)`. A failed mount never formats or retries
destructively. SD may still be read. Shared configuration and pin writers return
before file access when LittleFS is unavailable. With no readable storage, boot
continues using in-memory defaults. The compiled failure test covers these actual
entry points, including missing and partial pin configuration on SD.

Battery readings check I2C success, presence, voltage and SOC validity. Invalid
measurements preserve the last valid sample; an unknown startup value remains
unknown rather than becoming a low-battery alarm. Large changes and the low
threshold require consistent samples. Charging uses the charger's status register.
No calibration or protection parameters were changed by the repair.

Display Off stops the DELTA render and panel/backlight. Software sleep restores
CPU/display state and consumes wake input. Automatic idle does not enter deep
sleep. The existing explicit deep-sleep action shuts down radios/peripherals and
uses the board's existing wake pin. Physical wake reliability and current draw
require hardware verification; host checks do not measure power consumption.

## Build on this Windows workstation

Use the existing Python/PlatformIO installation; do not reinstall working tools.
The isolated tool packages live below `.tools/platformio-core`. The board config
pins its ESP32 platform and framework dependencies. Missing packages must be
downloaded from their official sources at those exact versions.

The branding pre-hook requires Pillow. This workstation's original interpreter
already has Pillow 12.3.0; the isolated PlatformIO interpreter needed that missing
package added separately. On a fresh setup, check the interpreter used by SCons
before installing it. Do not replace a working Python or driver installation.
WebUI resources are packed locally with standard-library gzip; source bytes are
preserved, with no external minification service.

```powershell
.\scripts\build.ps1 -Environment flux-delta -Python 'C:\Projects\FLUX\.venv\Scripts\python.exe'
```

`-Python` can point to another existing interpreter with PlatformIO; the
`FLUX_PYTHON` environment variable is also supported. Without either, the wrapper
uses this project's `.venv`. The wrapper rejects concurrent PlatformIO builds and
uses a project-local PlatformIO core when one exists. Use
`-Environment flux-t-embed-cc1101` for the original interface regression build.

The application-only image is `.pio/build/flux-delta/firmware.bin`. Build output
may also contain a merged distribution image: that image is not the application
image and must not be used for this installation. The application limit is
4,653,056 bytes. Never change partitions to accommodate a larger binary.

## Checks and preview

```powershell
$env:CXX='C:\Projects\FLUX-DELTA\.tools\zig\zig-x86_64-windows-0.14.1\zig.exe'
& 'C:\Projects\FLUX\.venv\Scripts\python.exe' -m unittest discover -s tests
& 'C:\Projects\FLUX\.venv\Scripts\python.exe' tools\delta_preview\preview.py
```

Open `http://127.0.0.1:8765`. The server compiles the shared renderer with Zig
0.14.1 below `.tools/zig` (available from the official Zig download site).
Pillow is needed to serve PNGs. Captures are in `assets/delta/preview`;
generate them with `preview.py --capture`. Hardware applications and status are explicitly
simulated in the preview. Browser arrows, Enter and Escape work when focus is on
the scene; controls retain their normal keyboard behavior.

Host render timings exclude LCD transfer and firmware work. Device lines starting
`[DELTA FPS]` report measured render/transfer costs and actual frame counts over
the measured interval. A configured frame interval is not proof of 60 FPS.

## Installation boundary

Only root controls the serial port. Before any write, recheck device identity,
security state, flash size, the current partition table, image metadata and the
private backup's size/SHA-256. Write only the verified application image inside
the existing factory partition, then verify and confirm boot separately. Never
erase the whole flash, write a merged image/bootloader/partition table, reset NVS,
or format LittleFS/SD. The backup stays outside this repository and source ZIP.

Build, installation and physical validation results are recorded in the delivered
validation report. No push or release is part of this task.
