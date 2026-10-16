# FLUX identity build verification

## FLUX target

- Environment: `flux-t-embed-cc1101`
- Result: SUCCESS in 350.23 s
- RAM: 130,784 / 327,680 bytes (39.9%)
- Flash: 4,200,154 / 16,777,216 bytes (25.0%)
- Factory image: `FLUX-flux-t-embed-cc1101.bin`, 4,266,240 bytes
- SHA-256: `eaff65ab169048fb48280c3b6f218d1fae33a774a5e9622d1f5b462c0c7ca485`

Two transient GitHub timeouts occurred while PlatformIO populated the new environment namespace. The third and final retry reused packages that had installed successfully and completed. No concurrent PlatformIO process was used.

## Bruce regression target

- Environment: `lilygo-t-embed-cc1101`
- Result: SUCCESS in 150.78 s
- Factory image: `Bruce-lilygo-t-embed-cc1101.bin`, 4,266,272 bytes
- SHA-256: `2bb60d7974c54afea81013e47b5f6f9d92c4eb9c3341ee81f21f60be0dc0500c`

The fallback retains Bruce branding and artifact prefix. The small binary difference from pristine baseline follows the shared identity expression; RAM is unchanged.

## Review outcome

Ollama supported the additive identity architecture. Its code review twice claimed the header/environment were missing because the first diff omitted untracked files and the second response failed to incorporate supplied context. Those findings are rejected by direct file inspection and successful builds of both environments. The reviewer was improved so future `--diff` context includes small untracked source files while excluding generated logs.
