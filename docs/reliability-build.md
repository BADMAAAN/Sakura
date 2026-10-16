# Target reliability verification

## Changes

- corrected SCK/MISO Arduino aliases in both T-Embed branches;
- removed an impossible lower-bound comparison on a uint16_t key delay;
- used portable PRIu32 formats for RAM diagnostics;
- left attack-module duration overflow unchanged pending a separate safety/product decision.

## FLUX target

- Result: SUCCESS in 318.39 s
- Warnings: 154 lines (baseline cold build: 180)
- Errors: 0
- RAM: 130,784 / 327,680 bytes
- Flash: 4,200,154 / 16,777,216 bytes
- Factory image: 4,266,240 bytes
- SHA-256: `7ba0ff1e2b7462e4a7c33c02049a2e81511e14b5e8d9b985bdca93558a3154d1`

## Bruce regression target

- Result: SUCCESS in 305.91 s
- Warnings: 154 lines
- Errors: 0
- Factory image: 4,266,288 bytes
- SHA-256: `6f42fb6c41ea49620b076fdf0121f264aebfb93bbbb4a3d70476524b1a60f1d2`

The config and RAM diagnostics targeted by this patch are absent. Two known karma_attack overflow warnings remain explicitly out of scope.
