# Collision and damage runtime — Mac 1.0.6

Status: **binary-confirmed clean reconstruction with bounded player/terrain edges**.

This document records the collision-candidate, radial geometry, player-impact,
shield-damage, and immediate ordinary-destruction path recovered from the
canonical Mac 1.0.6 PowerPC executable. Group teardown and destruction-side
consequences are documented separately in `DESTRUCTION_GROUP_RUNTIME.md`.

## Binary anchors

| Routine | Role |
| --- | --- |
| `0x12A00` | build integer player collision Rect |
| `0x12AD0` | build integer entity collision AABB |
| `0x14F10` | shield/damage/hit-effects routine |
| `0x16300` | ordinary destruction/effect path |
| `0x27100` | player shield/damage path; concrete clean mutation implemented |
| `0x34090..0x34314` | player-impact scan inside member update |
| `0x36120` | group/member teardown and reward consequences |
| `0x36AB0` | safe pointer/serial/active reference validation |
| `0x36CF0` | entity collision candidate scan and symmetric damage tail |
| `0x37580` | pickup dispatcher; concrete clean stat mutation implemented |
| `0x42F80` | quantized radial overlap helper |

## Collision domains and compiled fields

The Unit Definition loader derives compiled `UnitDef +0x08` from
`isGroundBased_BOOL`:

- true -> FourCC `grnd`;
- false -> FourCC `air `.

Collision-facing Unit Definition anchors are:

| Source key | Compiled offset | Runtime use |
| --- | ---: | --- |
| `harmlessToPlayers_BOOL` | `+0x11A` | candidate class partition |
| `playerProjectile_BOOL` | `+0x11B` | asymmetric projectile policy |
| `canBeHitByPlayerProjectile_BOOL` | `+0x11C` | projectile compatibility |
| `hittableWhenInvisible_BOOL` | `+0x121` | live collision participation |
| `isGroundBased_BOOL` | `+0x125` | derives `grnd` / `air ` domain |
| `collidesWithGroundObstacles_BOOL` | `+0x128` | separate ground-obstacle path |
| `damage_FLOAT` | `+0x274` | reciprocal collision damage |
| `hitParticles_ID` | `+0x2D8` | hit particle effect |
| `shields_BaseAmount_FLOAT` | `+0x43C` | base shields |
| `shields_LevelIncrement_FLOAT` | `+0x440` | context-dependent shield increment |
| `shields_MaxAmount_FLOAT` | `+0x444` | positive-increment clamp |
| `score_INT` | `+0x4B8` | destruction score value |
| `pickup_Type_ID` | `+0x4D4` | pickup branch discriminator |
| `pickup_Value_INT` | `+0x4DC` | pickup value |

Relevant state anchors include `collision_Spawn_ID +0x2E0`,
`collision_RepeatSpawns_BOOL +0x2E4`, `collision_SpawnDelay_INT +0x2E8`,
`passHitsToOwner_BOOL +0x32B`, `stateCollides_BOOL +0x347`,
`stateInvulnerable_ShieldsDoNotDepleteOnCollision_BOOL +0x348`,
`stateCollidesWithPlayers_BOOL +0x34F`, `stateDoNotGlowOnCollision_BOOL +0x354`,
`stateUseThisStateOnShieldDepletion_BOOL +0x356`,
`stateOnHitChangeStateDelay_INT +0x3B8`, and `stateOnHitChangeTo_STR +0x59C`.

## Integer AABB and exact radial geometry

`0x12AD0` converts each `x/y +/- halfExtent` edge independently with PPC
`fctiwz`, i.e. truncation toward zero. Strict separation rejects the pair;
touching AABB edges therefore continue to `0x42F80`.

`0x42F80` is **not** a sprite/mask test. The compatibility expression is:

```text
squared  = single(dx*dx + dy*dy)
q        = trunc_toward_zero(squared)
distance = float(sqrt(q))
hit      = distance < radius1 + radius2
```

Startup `0x429C0..0x42A00` fills a 16,384-entry `sqrt(i)` table for the small
integer-distance path. Entity radii are integer half-spans from already
truncated vertical AABBs. Player radii are `0.5f * truncatedRectHeight`, so
half-integer radii preserve observable quantization; a raw 6.5-distance
comparison can still hit because `42.25 -> 42 -> sqrt(42) < 6.5`.

## Entity candidate policy (`0x36CF0`)

The scan requires active collision state, candidate participation, distinct
serial, matching `grnd`/`air ` domain, **opposite** `harmlessToPlayers`
classes, non-positive group delay, and the executable's asymmetric projectile
filter before AABB/radial geometry.

A hit performs two damage legs. `passHitsToOwner` resolves through a validated
parent safe reference. The second leg preserves a Mac 1.0.6 quirk: it tests the
candidate flag but loads **self.parent**, not candidate.parent. Both damage calls
occur before the scan tests whether self became inactive.

## Damage and same-call ordering (`0x14F10`)

Canonical `Entity_HitDelay=1.0` is truncated to integer and uses strict:

```text
currentTick > lastCollisionHitTick + delay
```

Accepted damage updates the hit tick, subtracts shields, clamps at zero, and
calculates absorbed damage from the old/clamped values. Collision-invulnerable
states restore old shields after that calculation. On-hit state changes have a
separate strict delay gate.

The routine captures its compiled-state pointer before an on-hit transition and
continues using that old pointer for same-call glow/hit-particle/collision-spawn
fields. Collision-spawn delay accepts equality (`current >= last + delay`), and
non-repeat state is per entity.

When ordinary damage becomes lethal, callers with a `LegacyRemovalContext` can
run recovered `0x16300` effects immediately. This is required for correct
same-call destruction spawn/random-bonus ordering and RNG position. The later
`0x36120` teardown is idempotent with respect to already-processed effects.

## Shield construction (`0x35E50..0x35EB0`)

```text
shields = base
if levelIncrement > 0:
    shields = base + levelIncrement * (contextValue - 1)
    if shields > max:
        shields = max
```

The max clamp does not run when increment is non-positive. `contextValue` comes
from game-context `+0x14`; its final higher-level gameplay name remains bounded.

## Player-impact path

`0x34090..0x34314` requires status-4 players, entity `stateCollides_BOOL`, a
non-harmless Unit Definition, `stateCollidesWithPlayers_BOOL`, and the original
viewport bounds including `maxX >= -32`. Both player slots' active flags,
Rects, centers, and radii are snapshotted before iteration.

Ordinary impacts apply canonical Game.gafl index 161
`Player_ImpactDamageToEntities=100.0` to the entity side, optionally redirected
through `passHitsToOwner`. The player's side then receives UnitDef
`damage_FLOAT` through `0x27100` even if the preceding entity-side hit destroyed
the entity/owner. Player status is re-read afterward.

`pickup_Type_ID != none` selects the exclusive pickup path through `0x37580`.
A failed pickup performs no ordinary impact. A successful pickup invokes
ordinary destruction/consumption with the player's signed owner index, then
marks the pickup-consumed byte and skips reciprocal player damage. The clean
pickup/stat implementation now lives in `player_runtime.cpp`; the callback remains only
as a subsystem orchestration boundary.

## Validation and remaining boundary

Synthetic repository tests are **38/38 PASS**. Canonical Game.pak remains
stable at 386 groups / 546 constructed members, construction RNG seed
`2249411936`, 544 active members after the first player-aware tick, and motion
RNG seed `2633739833`.

Remaining collision-adjacent work is deliberately bounded to:

- world/audio/UI orchestration of the concrete player pickup/damage result facts;
- ground/terrain collision and actual obstacle/terrain mutation;
- renderer/terrain mutation beyond the recovered `0x16880` media route; the former live `+0x19` gate is now proven as the cached air-domain bit;
- full orchestration of remaining non-collision destruction entry sites.
