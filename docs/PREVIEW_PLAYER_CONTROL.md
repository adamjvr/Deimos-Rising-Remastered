# Preview Player Control Integration

## Scope

This runtime is the first host-input bridge used by the original-data live
Metal session. It is intentionally narrower than the final reconstructed input
system.

Canonical 1.0.6 data supplies the movement tuning:

- `Player 1[pl01].plde` `active_DefaultMaxSpeed_FLOAT = 7.8`;
- `Player 1[pl01].plde` `active_VelocityDelta_FLOAT = 1.6`;
- `Game[gafl]` slot 183, label-verified as `Player_TopGameAreaLimit = 13`;
- visible gameplay bounds remain the recovered 416x480 frame.

The clean integration API is in `preview_player_control.hpp/.cpp`. Opposing
modern host directions cancel, velocity converges component-wise toward
`+/-max_speed` by `velocity_delta`, position integrates once per 30 Hz preview
tick, and the integration fixture constrains the player to its visible game
rectangle.

## Evidence boundary

The tuning values and frame dimensions above are original-data evidence. The
exact original InputSprocket/film-bit-to-player-control dispatcher is **not yet
instruction-closed**. Therefore this API is deliberately named `Preview...`
and is not used to assign semantic names to `FilmInputBit` values.

This distinction lets native host/input integration advance without turning a
reasonable modern key mapping into fake reverse-engineering evidence. Once the
PPC input dispatcher and replay bit semantics are closed, the preview bridge can
be replaced or proven equivalent.

## macOS host mapping

The optional Apple live smoke app currently maps:

- Left Arrow / A -> left;
- Right Arrow / D -> right;
- Up Arrow / W -> up;
- Down Arrow / S -> down.

Key-down and key-up are tracked independently, so diagonal movement and
opposing-direction cancellation are represented. This is a modern developer
mapping only; it is not a claim about the original shipped keyboard defaults.

iPadOS remains presentation-only in this checkpoint; controller/touch mapping
will attach to the same portable control snapshot after the desktop integration
is validated.

## Deterministic validation

No-input canonical full-frame oracles remain:

- initial: `0x9e8a7ec73b79b254`;
- tick 1: `0x44dede08075273f2`;
- tick 30: `0x51d4a7eec9b0beef`.

A separately loaded Level-1 / Player-1 session with one right-input tick yields:

- player position `(209.6, 330)`;
- player velocity `(1.6, 0)`;
- terrain source top `3119`;
- complete 640x480 xRGB1555 FNV64 `0x6fd5c94a64dcb0c8`.

`preview_player_control_test` freezes tuning compilation, label drift rejection,
velocity convergence, release deceleration, opposing directions, bounds, and
inactive-player gating. `deimos_original_frame_probe` freezes the original-data
controlled-frame oracle in addition to the established no-input hashes.
