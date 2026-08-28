# Collision, player-impact, and damage runtime — Mac 1.0.6

This document records the clean-room reconstruction boundary for the canonical
Deimos Rising 1.0.6 PowerPC executable. It separates binary-confirmed behavior
from player/terrain/destruction work that is still intentionally bounded.

## Recovered entity/entity pipeline

The entity candidate scan is PPC `0x36CF0`. Before geometry, the original
requires active collision states, candidate participation, a different serial,
matching collision domain (`grnd` or `air `), **opposite**
`harmlessToPlayers` classes, non-positive group delay, and its asymmetric
player-projectile policy.

PPC `0x12AD0` forms integer bounds by converting every `x/y +/- half extent`
edge independently with `fctiwz`. Separation is strict, so touching AABB edges
survive to the radial stage.

PPC `0x42F80` is not a sprite-mask routine. Its observable compatibility test is:

```text
squared  = single(dx*dx + dy*dy)
q        = trunc_toward_zero(squared)
distance = float(sqrt(q))
hit      = distance < radius1 + radius2
```

Startup `0x429C0..0x42A00` fills a 16,384-entry table with `sqrt(i)` for the
small-distance fast path. Entity radii are signed integer division by two of
the already-truncated vertical AABB span.

After overlap, damage is symmetric. Both legs use PPC `0x14F10`, but the
second `passHitsToOwner` path has a 1.0.6 compatibility quirk: it tests the
candidate flag and then resolves **self.parent**, not candidate.parent. The
clean runtime preserves this rather than normalizing it.

## Damage semantics

`Entity_HitDelay` (Game.gafl index 167, canonical value 1.0) is tested as
strict `currentTick > lastHit + delay`. Accepted damage updates the last-hit
tick, subtracts from shields, and clamps shields at zero. A collision-
invulnerable state restores the previous shield value after calculating the
absorbed amount.

On-hit state transition has a separate strict delay. PPC `0x14F10` retains the
compiled-state pointer captured before that transition, so glow, hit-particle,
and collision-spawn decisions later in the same call use the **pre-transition**
state. Collision-spawn delay accepts equality (`current >= last + delay`), and
non-repeat state is tracked per live entity.

When ordinary clean-room shield depletion reaches zero, the headless runtime
records lifecycle/source/score facts. Full `0x16300` audiovisual, child,
terrain, obstacle, reward, and group consequences are intentionally not
invented yet.

## Shield initialization

Constructor code `0x35E50..0x35EB0` implements:

```text
shields = base
if levelIncrement > 0:
    shields = base + levelIncrement * (contextValue - 1)
    if shields > max:
        shields = max
```

The max clamp does **not** execute when the increment is non-positive. The
context value comes from PPC `0x5CD0`, which returns game-context `+0x14`; its
higher-level gameplay name remains unresolved.

## Player collision geometry

The player helper `0x12A00` forms the same truncated Rect from player center and
half extents. The main tick computes player radius as exactly
`0.5f * (Rect.bottom - Rect.top)`, unlike integer entity radius. It then calls
the same `0x42F80` radial helper.

This makes quantization observable. For example, a raw center distance of 6.5
against a 6.5 radius sum is still a hit: `6.5^2 = 42.25`, `fctiwz -> 42`, and
`sqrt(42) ~= 6.481 < 6.5`.

## Player-impact control flow

PPC `0x34090..0x34314` is represented by
`scan_legacy_player_collisions()` as a bounded headless scanner. Preconditions
are:

- at least one status-4 player;
- current state `stateCollides_BOOL`;
- Unit Definition is not `harmlessToPlayers_BOOL`;
- current state `stateCollidesWithPlayers_BOOL`;
- entity bounds satisfy `maxX >= -32`, `minX <= viewportMaxX`, `maxY >= 0`,
  `minY <= viewportMaxY`.

The original snapshots both player active flags, Rects, centers, and radii
before looping slot 0 then slot 1. Each slot does AABB and radial overlap.

For ordinary entities, `passHitsToOwner` may redirect the **entity-side** hit.
That target receives Game.gafl index 161 (`Player_ImpactDamageToEntities`,
canonical 100.0). The player then receives the colliding entity's own UnitDef
`damage_FLOAT` through player routine `0x27100`. This reciprocal player-damage
call still occurs if the preceding entity-side hit destroyed the target. The
player's status byte is re-read afterward; status 4 remains the active value.

The clean collision layer exposes player damage as a callback because the full
player shield/life/state machine is a separate reconstruction boundary.

## Pickup branch

The non-ordinary branch at `0x341D0` is pickup handling, not a collision-mask
or media-impact mode. UnitDef compiled fields are:

- `+0x4D4` -> `pickup_Type_ID`;
- `+0x4DC` -> `pickup_Value_INT`.

`0x37580` dispatches pickup categories. Canonical Game.pak uses 8 non-`none`
pickups: 4 `coin`, 2 `shie`, 1 `exli`, and 1 `mult`.

A pickup collision is exclusive:

1. invoke pickup dispatcher;
2. if it returns false, end that slot with no ordinary impact;
3. if it returns true, destroy/consume the entity using the player's signed
   owner/index byte;
4. do not damage the player.

Concrete inventory/weapon/stat changes remain behind a callback until
`0x37580`'s player-side callees are reconstructed.

## Current validation

The repository test suite is **21/21 PASS**. Canonical Game.pak validation still
produces:

- 386 groups / 546 live members after construction;
- construction RNG seed `2249411936`;
- 544 active members after the first player-aware tick;
- motion RNG seed `2633739833`;
- 436 collision-enabled states;
- 151 player-collision states;
- 135 air / 251 ground collision domains;
- 8 pickup Unit Definitions.

## Next binary boundary

The next collision/destruction work is deliberately concentrated in:

- `0x16300`: full destruction side effects;
- `0x36120`: group/member removal, coins/group-kill reward, child handling;
- concrete `0x37580` pickup mutation;
- concrete `0x27100` player damage/life semantics;
- ground/terrain collision and terrain mutation.

Several destruction offsets are already strongly correlated, but the remaining
`+0x4B2..+0x4B4` boolean cluster will not be assigned final semantic names until
all terrain/obstacle/random-bonus callees are correlated.
