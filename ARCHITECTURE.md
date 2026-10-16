# FLUX Architecture

Status: discovery draft. The final architecture will be recorded after the Bruce baseline build and a bounded Codex/Ollama design review.

## Current direction

- Keep Bruce history and source layout intact.
- Track `BruceDevices/firmware` through the `upstream` remote.
- Develop FLUX on `flux/main` with small, reviewable commits.
- Prefer additive FLUX seams, configuration and branding over broad renames or rewrites.
- Treat the LILYGO T-Embed CC1101 Plus board layer as the hardware source of truth only after cross-checking Bruce, LILYGO schematics/source and relevant chip documentation.
- Track flash, static RAM and PSRAM impact for feature work.
- Route FLUX branding and Tamagotchi artwork through the versioned asset manifest described in
  `docs/visual-assets.md`; feature code consumes logical asset IDs, never source filenames.

## Repository additions

- `ai/`: reproducible local Ollama reviewer workflow and review records.
- `docs/decisions/`: durable architecture decision records.
- `scripts/`: build and verification entry points.
- `tests/`: host-side tests where practical.

Upstream firmware internals remain unchanged until the baseline is proven.
