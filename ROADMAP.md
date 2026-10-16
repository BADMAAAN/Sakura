# FLUX Roadmap

1. Environment audit — complete
2. Upstream pin and reproducible baseline build — complete
3. Hardware/source mapping for T-Embed CC1101 Plus
4. Bounded Codex/Ollama architecture decision
5. Merge-friendly FLUX identity
6. Core integration seams
7. UI improvements
8. Hardware feature reliability
9. Memory, flash and concurrency optimization
10. Automated verification and warning review
11. Release-candidate build and documentation
12. Replaceable FLUX visual assets
    - accept the final FLUX firmware icon and generate a compact embedded fallback;
    - accept an optional splash/branding image with SD -> LittleFS -> embedded fallback resolution;
    - validate dimensions, formats, memory budget and manifest compatibility during the build;
    - keep temporary/missing artwork clearly non-final until user-supplied assets arrive.

Feature scope will be refined from evidence gathered during baseline and architecture review. No feature work precedes a successful baseline build.

## FLUX v0.1 release scope

- polished, responsive vertical menus while preserving inherited icons and themes;
- replaceable FLUX splash, firmware icon, and secondary icon overrides;
- persistent UI and encoder-wheel LED customization;
- preserve `Others` (Oddness) behavior;
- hide NRF24, FM, and GPS only in the FLUX build;
- FLUX plus upstream regression builds, tests, review, and release-candidate documentation.

## Future / FLUX v0.2+

- Tamagotchi system;
- animated heroes;
- progression and game mechanics.

Future features do not start until the stable FLUX v0.1 release candidate is complete.
