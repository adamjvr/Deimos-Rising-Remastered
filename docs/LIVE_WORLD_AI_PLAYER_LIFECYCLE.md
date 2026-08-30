# Live World AI and Player Lifecycle Integration

## Scope

This document records the native live-world corrections made after macOS playtesting exposed two integration gaps: Player 1 could remain permanently in the recovered dying state, and enemy state machines advanced without selecting their serialized/directional sprite frames. These corrections deliberately reuse recovered runtimes rather than inventing modern substitutes.

## Player lifecycle

Entity/player collisions already call the recovered damage path. Fatal damage writes legacy player status `3` (`dying`) and records the death tick. The host movement bridge accepts input only in status `4` (`active`), which is correct. The missing operation was the outer per-tick call to the recovered lifecycle switch (`0x2A150`).

`OriginalGameFramePreview::tick_live()` now calls `advance_legacy_player_lifecycle()` before movement. While dying, movement is inert and player weapon input is explicitly gated. Once the recovered dying deadline expires, one life is consumed; if lives remain, active state and default shield are restored and movement resumes. The canonical Level-1 integration oracle enters dying at tick 203 and respawns at tick 284 with two lives remaining.

## Enemy state and target-facing visuals

The existing entity runtime already advances target refresh, Hunt, Hold, Cyclic, Flee and state timers. The visual bridge previously initialized every live unit to sprite frame zero, hiding much of that behavior and using incorrect orientations even for fixed-frame states.

Each live visual now starts a state from `stateSpriteFrameMin_INT`. For states with `stateDoRotateToTarget_BOOL`, the canonical 1.0.6 corpus uses 36 directions × one frame per direction. The already-recovered integer point-angle helper supplies the legacy heading convention, and the visible frame is the truncated directional index. Cardinal witnesses are 0°/90°/180°/270° → frames 0/9/18/27. Rotation pauses suppress facing updates.

This does **not** make Hunt into homing movement. The recovered Hunt routine keeps its random velocity-envelope semantics. It also does not equate the separate rule fact `Is Tracking Player` with target presence; that rule remains a reverse-engineering frontier.

## Canonical external-data witnesses

The corrected 120-tick Level-1 run starts with 151 canonical members and records 17,906 target observations, 3,004 target-facing visual updates, 211 state changes and two entity/entity collisions. Full-frame integration witnesses are:

- initial live world: `0x864f27d9c3820d7f`
- first air-fire tick: `0x1a19a251ec90428c`
- tick 120: `0xc5ccb82cfec84a72`

The prior static/no-input/player-control witnesses remain unchanged.
