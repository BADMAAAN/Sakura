# ADR 0004: Correct target pin aliases and type warnings

## Problem

Official LILYGO mapping and Bruce macros define MISO/SCK correctly, but Arduino static aliases are reversed in both branches of the board header. Baseline also reports an impossible unsigned comparison and non-portable uint32_t printf formats.

## Codex proposal

Correct the aliases, remove only the impossible lower-bound check, and use standard PRIu32 format macros.

## Ollama criticism

Ollama independently confirmed the SCK/MISO mismatch and recommended correction plus later physical verification. It did not identify a regression in the two type-only fixes.

## Codex response

Both board branches are corrected because both repeat the same alias defect. Macro pin values and all bus-specific definitions remain unchanged. The upper delay clamp remains. RAM values and logging content remain unchanged.

## Alternative options

- Leave misleading aliases because current paths often use explicit macros.
- Cast uint32_t values to an ABI-specific integer type.
- Change attack-module duration types in the same patch.

## Final decision

Apply the three narrow fixes. Exclude the attack duration overflow because changing it affects offensive-module behavior and requires a separate safety/product decision.

## Impact

Generic Arduino SPI consumers receive correct aliases and seven source warnings should disappear. Physical validation remains pending explicit permission.
