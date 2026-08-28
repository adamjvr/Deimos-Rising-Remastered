# Entity core-edge runtime — air-domain obstacle gate and shield-depletion states

Status: **Mac 1.0.6 binary-confirmed and implemented in the headless core**.

This milestone closes two small live-member flags that had remained deliberately
unnamed because their meanings were not yet source-to-runtime proven: live
`+0x19` in the ground-obstacle path and live `+0xCD` in the zero-shield branch.
Both are constructor-derived caches of already-serialized Unit Definition facts;
neither is a separate gameplay mode.

## Live `+0x19`: cached air-domain flag

Base object construction initializes live `+0x19` to one. The live-member
constructor at `0x35F88..0x35FA0` then overwrites it by comparing derived
`UnitDef +0x08` with FourCC `air `:

```text
live+0x19 = (UnitDef+0x08 == 'air ')
```

`UnitDef +0x08` was already proven to be the loader-derived collision domain:
`grnd` when `isGroundBased_BOOL` is true and `air ` otherwise.

The main member update tests the cached byte before
`collidesWithGroundObstacles_BOOL`:

```text
if stationary:                         skip obstacle query
if live+0x19 != 0:                     skip obstacle query   # air domain
if !collidesWithGroundObstacles_BOOL:  skip obstacle query
if persistent_rects.overlap(rect):
    velocity = {0,0}
    stationary = true
```

The clean `legacy_collides_with_ground_obstacle()` now enforces the same ground
collision-domain requirement before querying the persistent rectangle store.
No additional clean live byte is necessary because `CompiledUnitBehavior`
already contains the exact derived FourCC.

## State `+0x356`: shield-depletion selector

State parser `0x41698..0x416A8` calls the Boolean parser with destination
`state +0x356`. Relocated initialized strings resolve that exact call to:

```text
#stateUseThisStateOnShieldDepletion_BOOL
```

The neighboring calls provide an independent sequence check:

```text
state +0x353  #stateDrawToTerrain_BOOL
state +0x354  #stateDoNotGlowOnCollision_BOOL
state +0x356  #stateUseThisStateOnShieldDepletion_BOOL
state +0x355  #stateUseThisStateOnWeaponPowerupRelease_BOOL
```

The unusual `+0x356`/`+0x355` parse order is present in the executable and is
preserved as evidence rather than reordered to match serialization.

## Live `+0xCD`: cached existence of a shield-depletion state

Member constructor `0x35DAC..0x35DF0` initializes live `+0xCD` to zero, scans
all compiled states in file order, tests `UnitDef +0x836 + stateIndex*0x5E0`
(the UnitDef-relative form of state `+0x356`), and sets `+0xCD = 1` on the first
marked state.

The clean compiler records the same derived fact as
`CompiledUnitBehavior::has_shield_depletion_state` and retains the per-state
`use_on_shield_depletion` flag.

## Zero-shield dispatch: `0x14F10 -> 0x17E70`

After accepted collision damage reaches zero shields, `0x14F10` first performs
the recovered score award. It then tests live `+0xCD`:

```text
score += UnitDef.score

if live+0xCD == 0:
    ordinary destruction 0x16300
else:
    shield-depletion state dispatcher 0x17E70
```

`0x17E70` scans states in file order. On the first state whose `+0x356` byte is
nonzero it calls state-entry routine `0x146F0` for that state and returns. It
does **not** run ordinary destruction `0x16300` on that branch. If no marked
state exists, the function simply returns; the constructor cache makes that
case unreachable for a normally constructed member.

The clean damage result therefore exposes both
`shield_depletion_state_entered` and the selected state index. State entry uses
the existing exact state-entry machinery so timer/counter/spawn RNG behavior
continues through the same path as any other state transition.

## Canonical corpus coverage

The stock Mac 1.0.6 `Game.pak` contains:

- 386 Unit Definitions;
- 1,167 states;
- 135 `air ` Unit Definitions and 251 `grnd` Unit Definitions;
- 4 units with `collidesWithGroundObstacles_BOOL`;
- **0** states marked `stateUseThisStateOnShieldDepletion_BOOL`;
- **0** units whose constructor would set live `+0xCD` from that marker.

So the `0x17E70` path is executable- and format-supported compatibility
behavior but is not exercised by stock campaign content. Synthetic regression
coverage is therefore required and is included in `core_edge_runtime_test`.

## Validation

This milestone raises the repository suite to **26/26 PASS**. The external
canonical `Game.pak` probe also retains the established deterministic baseline:

```text
386 groups / 546 live members
construction RNG seed 2249411936
544 active after first tick
motion RNG seed 2633739833
```

## Remaining boundary

These two live flags are closed. Remaining adjacent work is renderer/bitmap and
terrain-image mutation beyond the proven persistent Rect store, world-level
orchestration of player lifecycle effects, and routing the remaining
non-collision destruction entry sites through the recovered teardown layer.
