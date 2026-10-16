# Building FLUX

For the locally rebuilt DELTA target, current tool selection, preview and
application-only installation boundaries, see [FLUX DELTA](docs/FLUX-DELTA.md).
The historical baseline measurements below describe the original project.

## Baseline target

- Board: LILYGO T-Embed CC1101 Plus
- Upstream PlatformIO environment: `lilygo-t-embed-cc1101`
- Pinned upstream commit: `ba519c936c87b89c9c667c15b932d7faca5360d5`

## Verified environment

- Python 3.14.7 in project-local `.venv`
- PlatformIO Core 6.1.19
- SCons 4.8.1 / `tool-scons @ 4.40801.0`
- `pioarduino/platform-espressif32` 55.03.39

## Verified command

```powershell
.\scripts\build.ps1
```

The script rejects concurrent PlatformIO processes and invokes the target through `scripts/pio.py`. That wrapper selects a healthy official PlatformIO mirror for this process only. Two successful builds, hashes, warnings, and the workaround are documented in `docs/baseline-build.md` and ADR 0002.

The default is `flux-t-embed-cc1101`. To run the inherited upstream regression target:

```powershell
.\scripts\build.ps1 -Environment lilygo-t-embed-cc1101
```

No upload target may be run without explicit user approval.

## Fast host-side checks

```powershell
.\.venv\Scripts\python.exe -m unittest discover -s tests -v
```

These source invariants catch identity, target inheritance, SPI alias, artifact fallback, and build-concurrency regressions. They complement rather than replace PlatformIO builds or later approved hardware tests.
