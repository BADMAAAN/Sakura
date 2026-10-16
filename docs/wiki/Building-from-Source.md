# Building from Source

## Prerequisites

- Git
- PlatformIO Core or the PlatformIO IDE extension
- A supported Python runtime supplied by PlatformIO

From the repository root, build the target with:

```powershell
pio run -e lilygo-t-embed-cc1101
```

The board configuration and partition layout live under `boards/` and
`platformio.ini`. Build outputs are written to `.pio/` and are ignored by Git.

Do not commit generated firmware binaries to the source repository. Versioned
release artifacts are published separately only after hardware verification.
