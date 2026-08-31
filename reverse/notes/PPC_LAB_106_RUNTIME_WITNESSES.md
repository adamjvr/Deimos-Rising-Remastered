# Deimos Rising 1.0.6 — PPC Lab Runtime Witnesses

Date: 2026-08-30

This note records target-safe behavioral evidence recovered from the shipped PowerPC executable. The original executable bytes are external evidence and are deliberately not stored in this repository.

## Target identity

- Application: Deimos Rising 1.0.6
- Container: PowerPC PEF/CFM application data fork
- Extracted data-fork length: 2,045,976 bytes
- SHA-256: `8e436c3babc582f1407ae6fed47e9749f1c930335ce4c794947e40b06b85eb29`
- PPC Lab TOC used for internal-call probes: `0x100e6330`

## Weapon charge/release — handler 0x3B3C0

Recovered compiled Weapon Definition offsets:

| Offset | Meaning |
|---:|---|
| `+0x1E8` | TimeUntilActivation |
| `+0x1EC` | ActivationSpawn_ID |
| `+0x1F0` | TimeBetweenPowerLevelChanges |
| `+0x1F4` | MaxPowerLevel |
| `+0x1F8` | OverloadTime (serialized; not an automatic-release read in this handler) |
| `+0x1FC` | ReleaseSpawn_ID |
| `+0x200` | TimeBetweenReleaseSpawns |
| `+0x204` | DoReleaseOnMaxPowerLevel |

The release path emits one `ReleaseSpawn_ID` at each release interval and decrements the stored integer power level once after every emission. At zero the charge/release state resets.

A direct PPC Lab execution witness used `OverloadTime=1`, a hold long past that duration, current power=max power=20, and `DoReleaseOnMaxPowerLevel=false`. The routine returned with charge state still active, power level still 20, and percentage `100.0f`. Therefore the earlier clean-host overload-time auto-release was not shipped 1.0.6 behavior and was removed.

## Main member movement/lifetime — routine 0x12CA0

The main entity tick calls `0x12CA0` with margin `128` and mode `1`, then immediately marks the member deleted when the routine returns false. This occurs before later obstacle/state/player/entity-collision processing.

PPC Lab boundary execution with viewport `416x480` and collision half-extents `10x10` produced:

| Position | Result |
|---|---:|
| `x=-139, y=100` | reject |
| `x=-138, y=100` | survive |
| `x=554, y=100` | survive |
| `x=555, y=100` | reject |
| `x=100, y=-129` | reject |
| `x=100, y=-128` | survive |
| `x=100, y=618` | survive |
| `x=100, y=619` | reject |

This closes the post-clean-movement lifetime predicate as:

- `x + halfWidth >= -128`
- `x - halfWidth <= viewportWidth + 128`
- `y >= -128`
- `y - halfHeight <= viewportHeight + 128`

Equality survives. The top test is intentionally asymmetric and does not add half-height.

## Collision spawn request — damage routine 0x14F10

The collision-spawn construction block at `0x1516C..0x1525C` copies the canonical 44-byte constructor request template and overwrites:

- `+0x00`: pre-hit state's collision Spawn ID
- `+0x04`: damaged target X
- `+0x08`: damaged target Y
- `+0x14`: damaged target player-owner byte
- `+0x20/+0x24`: damaged target pointer/serial safe parent reference

PPC Lab dump of the default request confirms no world-Y subtraction, no explicit heading, owner `-1` before overwrite, stationary=false, terrain-effects=false, and initial velocity multiplier `1.0f`.

The clean collision runtime now returns this complete `SpawnRequestSeed`; the owning world defers actual vector-growing construction until stable collision traversal has ended while preserving first-damage-leg then second-damage-leg request order.

## Regression witness after translation

- synthetic CTest suite: 53/53 PASS
- canonical `Game.pak` clean-core validator: PASS
- static original-frame hashes unchanged
- live initial FNV64: `0x1eb1e07d4b6d038d`
- live first-fire FNV64: `0xa0fc41ac06687be2`
- live tick-120 FNV64: `0x055b51228f651199`
- tick-120 max active: 19
- playable crash: dying tick 185, respawn tick 266, lives 2
- Plasma Bomb secondary fire: PASS
- charge activation: 15 ticks; `icps` release: PASS
- 3000-tick stress: max resident 114, final resident 10, pruned 1862, exact lifetime culls 236
