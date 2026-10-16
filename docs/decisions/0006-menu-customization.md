# FLUX menu customization and wheel LED architecture

Status: Accepted

## Constraints

- Keep Bruce's procedural menu icons, theme fallback, and the existing `Others` (Oddness) menu intact.
- Add no UI framework and avoid full-screen sprites or large frame buffers.
- Keep encoder input responsive and persist every user-facing customization across reboot.
- Use only the board definitions and LED abstraction already present for the T-Embed CC1101 family.
- Hide NRF24, FM, and GPS from FLUX builds without deleting their upstream implementation.

## Codex proposal

### Menu motion

Add a FLUX-only animated vertical submenu carousel while leaving Bruce's icon main menu and theme
renderer intact. The active item remains centered; optional previous/next entries are drawn above and
below. A menu change records the prior index and direction, then renders a short bounded transition:

- Off: one final frame;
- Fast: three linear frames with a short fixed duration;
- Smooth: 5-10 frames using integer smoothstep easing, with duration selected by the speed setting.

The renderer clears only the submenu content rectangle below the title, not the whole screen. Bruce's
procedural icons and theme-image main menu remain on their current renderer. Encoder steps continue to be
drained/coalesced before rendering, transitions use only 3-10 one-tick frames, and no per-frame heap
allocation or UI dependency is added.

### Persistent customization

Add a `Config -> Customization` submenu and serialize these FLUX settings through `BruceConfig`:

- animation mode (`Off`, `Fast`, `Smooth`);
- animation speed (bounded 1-10);
- active icon emphasis;
- neighboring icon visibility;
- wheel LED enabled.

Wheel brightness and effect reuse Bruce's existing persisted `ledBright` and `ledEffect` values rather
than introducing duplicate state. The wheel-specific controls are compiled only when `HAS_ENCODER_LED`
and `HAS_RGB_LED` are available. Disabling the wheel sets hardware brightness to zero without destroying
the saved brightness; enabling it reapplies the configured brightness and effect.

### Hardware boundary

For `lilygo-t-embed-cc1101`, Bruce defines eight WS2812B LEDs on GPIO14 (`LED_COUNT=8`, `RGB_LED=14`,
`HAS_ENCODER_LED=1`) and drives them through its FastLED-based `led_control` module. FLUX will call that
module only; it will not duplicate a pin number or access the strip directly. Existing supported effects
remain the effect list presented to the user.

### Feature trimming

The FLUX PlatformIO environment defines `FLUX_HIDE_NRF24`, `FLUX_HIDE_FM`, and `FLUX_HIDE_GPS`. The main
menu construction uses those flags to omit the three entries. Their source, board support, libraries, and
Bruce behavior remain untouched for upstream and non-FLUX environments.

### Verification

- Host tests validate defaults, JSON round-trip fields, menu gating, icon/Others preservation, and the
  animation implementation's bounded modes/easing.
- Build both the FLUX environment and the upstream Bruce environment.
- Run the proposal through local `qwen2.5:14b`; incorporate blocking findings before implementation.

## Ollama review and final decision

Local `qwen2.5:14b` returned `APPROVE` with no blocking findings. Its non-blocking requests were explicit
speed clamping, encoder responsiveness checks, confirmation of allocation-free frames, and verification
that wheel LED Off/On restores saved brightness/effect. These checks are part of implementation tests.

Final decision: accepted with those four review recommendations incorporated.
