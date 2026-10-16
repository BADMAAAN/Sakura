# DELTA shared-renderer preview

The native preview compiles `host.cpp` against the exact header and indexed
assets used by the firmware (`include/delta/core.h`, `artwork.h`). Browser code
does not redraw or approximate the Sakura scene. Hardware applications are not
emulated; Enter reports a simulated section request outside the device image.
Clock, battery and radio values in this preview are explicitly simulated.

From the project root, with the existing local Python interpreter:

```powershell
& 'C:\Projects\FLUX\.venv\Scripts\python.exe' tools\delta_preview\preview.py
& 'C:\Projects\FLUX\.venv\Scripts\python.exe' tools\delta_preview\preview.py --capture
& 'C:\Projects\FLUX\.venv\Scripts\python.exe' -m unittest discover -s tests -p test_delta_core.py
```

Portable Zig must be present below `.tools/zig`. The preview binds only to
`127.0.0.1:8765`. Arrow keys rotate, Enter records the section request, Escape
returns. Theme, particle density, motion quality, transition speed, reduced
motion, glitch, ambient clock, icons and status can be changed live.

`generate_artwork.py` reproduces the original procedural art from a fixed seed,
using already installed Pillow and the Windows Segoe UI font. It creates one
320×170 indexed scene, three 64-color palettes, thirteen 40×40 alpha icons and
ASCII glyph coverage. The firmware needs only the generated header. The source
PNGs, generator and header are retained together for the full source archive.
No downloaded art, external services or generated runtime dependencies are used.

Captures include native and 4× PNGs, all thirteen sections, ambient clock, an
animated navigation GIF and a JSON CPU-render benchmark. CPU timing excludes
LCD transfer, firmware processing and image encoding. It is **not hardware FPS**.
