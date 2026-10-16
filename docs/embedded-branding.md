# Embedded branding for flux-t-embed-cc1101

Sources live in `flux/`:

| File | Role | Original size |
| --- | --- | --- |
| photo_2026-09-15_01-51-18.jpg | splash (hands) | 1448 x 1086 |
| photo_2026-09-15_01-51-21.jpg | secondary (wide artwork) | 1672 x 941 |
| photo_2026-09-15_01-51-23.jpg | logo / device info | 1254 x 1254 |

Install Pillow into the PlatformIO Python environment before building (`python -m pip install Pillow`).
The target's pre-build script `scripts/embed_flux_branding.py` fits the complete artwork into 320x170
and 170x320 canvases using contain and black padding. These monochrome pictures are stored as
8-bit luminance arrays (326400 bytes total) in PROGMEM; the header is generated under the build directory.
Rendering converts luminance to RGB565 once when an image is shown. There is no runtime scaling.

The physical FLUX build shows the three public artwork frames first (`secondary`, `splash`, and
`icon`), then shows the private splash image and keeps it on screen until the first rotary-encoder
movement. The private image is not part of the repository: the local override lives under
`flux/private/`, which is ignored by Git. It uses a full-screen cover fit for the landscape display.
Other targets retain their existing startup path.
Device Info uses the logo. The embedded branch bypasses SD, LittleFS, legacy boot files and themes.
If buffer allocation fails, it displays FLUX text, never the upstream bitmap. Other targets retain their
existing startup path. Physical board rotation 3 gives a logical 320x170 screen.

The clean menu path composes status and content in a 16-bit screen-sized sprite (108800 bytes).
Main icons route only their drawing primitives to that buffer, without redirecting the global display.
Submenu content clips at y=42 through the final scanline. Its animated underline and separate scrollbar
were removed. Each completed frame uses one opaque push; animation timing has no added delays.

Neighbor previews default to OFF. Configuration without menuLayoutVersion >= 2 migrates old defaults
to OFF; subsequent explicit choices persist and affect both menus. In single-item mode even animation
shows only the current icon/title, with a short vertical ease-in. Existing physical input accumulation is reused.

Host tests cover asset preparation; firmware compilation covers the render code. Fast encoder scrolling,
panel tearing and boot appearance require physical visual testing after flashing.
