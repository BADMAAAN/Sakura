# FLUX Project Status

- PROJECT: FLUX
- CURRENT PHASE: FLUX v0.1 full-rebranding build is GREEN and locally validated
- DONE: Native Codex -> Aider supervisor; persistent continuation context; FLUX product/API rename; smooth vertical menu customization; wheel LED settings; FLUX-only feature hiding; replaceable branding bundle; FLUX build; host tests; Ollama review
- IN PROGRESS: Checkpoint and local commit for `flux-next-safe-triage`
- NEXT: Post-commit source/binary rebranding audit and upstream regression build
- BLOCKERS: None for the active native Windows workflow
- LAST SUCCESSFUL BUILD: 2026-08-26 FLUX SUCCESS in 52.56 s incremental / 444.48 s full (RAM 130,800 B; flash 4,205,334 B; merged artifact 4,271,408 B); 21/21 host tests PASS; qwen2.5:14b reviewer PASS
- CURRENT BRANCH: `flux/main`
- UPSTREAM COMMIT: `ba519c936c87b89c9c667c15b932d7faca5360d5`
- LAST LOCAL COMMIT: `feat: migrate FLUX supervisor to native Windows` (this change set)
- DATE: 2026-08-26 (Europe/Moscow)

## Known problems

- PlatformIO default endpoints returned HTTP 502/timeouts. A documented process-local official-mirror workaround is active.
- Baseline warnings include an overflow in `karma_attack.cpp`, unsigned comparison in `config.cpp`, format mismatch in `ram_profile.cpp`, and vendor LTO/string warnings.
- Cold and cache-assisted images have identical size/memory metrics but different hashes; byte-for-byte determinism is not yet established.
- SCK/MISO static aliases were corrected in both T-Embed branches and both targets build successfully; physical confirmation remains pending.
- Hardware behavior cannot be verified without a later, explicitly approved physical flash/test session.
- OpenHands remains an optional future backend and is not part of the active executor order or task gating.
- Safe warning triage removed deprecated implicit lambda captures, exhaustive WebSocket switch warnings, and an always-true RFID byte-range guard; behavioral radio/attack warnings remain deferred.
- Replaceable FLUX visual asset architecture now includes the supplied optimized splash, main-icon source, and secondary branding override; future game-specific assets remain deferred to v0.2+.
- The upstream target name does not distinguish the Plus product name; its board implementation nevertheless contains explicit `NRF24 on Plus`, PN532, BQ25896, BQ27220, CC1101 and speaker paths. This must be validated against official hardware sources and build output.

## Constraints

- No Git push or physical flashing without explicit user approval.
- Keep upstream history intact and changes merge-friendly.
- GLITCH remains out of scope until FLUX reaches a reviewed release candidate and the user authorizes transition.
