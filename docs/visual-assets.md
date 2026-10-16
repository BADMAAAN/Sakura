# FLUX visual asset architecture

## Status

Architecture and replacement seams are ready. No temporary artwork in this tree is a final FLUX asset.
The final icon and optional splash image will be supplied separately. Tamagotchi visuals are deferred to
FLUX v0.2+ and have no runtime engine or state contract in v0.1.

## Chosen pipeline

Use a hybrid pipeline:

1. Keep editable originals under `assets/flux/source/`.
2. Describe logical assets in `assets/flux/manifest.json`, copied from the committed example.
3. Generate only the small firmware icon into a C++ header for an always-available identity mark.
4. Package splash and secondary icon overrides under `/flux/assets/` on LittleFS or SD.
5. Resolve runtime assets in this order: SD override, LittleFS pack, compiled fallback.

This keeps the firmware usable without removable storage while allowing large visuals to be replaced
without touching UI or game logic. Generated headers and packaged output are build artifacts, not
hand-edited source.

## Formats

- Firmware icon: indexed or 1-bit source converted to a small `PROGMEM` array. RGB565 is allowed only
  when its measured flash cost is acceptable.
- Splash: JPEG for photographic/full-screen stills, GIF for short animation, PNG for transparency.
  The existing renderer already supports these formats. Target-specific dimensions belong in the manifest.
- Future animated content will be profiled separately for v0.2+; v0.1 adds no sprite animation engine.

## Stable runtime contract

Code asks an asset resolver for logical IDs:

- `branding.firmware_icon`
- `branding.splash`
- `icons.<menu-id>` for optional FLUX secondary overrides

The resolver returns a filesystem/path pair or a compiled fallback descriptor. Tamagotchi state and game
logic must not contain `.gif`, `.png`, or physical directory names. Missing optional assets fall back to
the existing text/procedural UI; they never prevent boot.

## Layout

```text
assets/flux/
  manifest.example.json
  source/
    branding/icon/
    branding/splash/
    future/
  generated/                 # ignored build output
sd_files/flux/assets/        # future packaged runtime output
```

The manifest version is checked by `scripts/validate_flux_assets.py`. A later conversion step should use
deterministic tools, record source hashes, reject oversized/dimensionally invalid files, and emit a size
report before compilation.

## Replacement workflow

When artwork arrives, place it in the matching source directory, copy `manifest.example.json` to
`manifest.json`, replace `pending` entries with paths and metadata, run the validator/converter, then build
both FLUX and Bruce targets. Replacing art must not require edits outside the manifest and asset directory.
