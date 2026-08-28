# Destruction and group-removal runtime — Mac 1.0.6

Status: **bounded binary-confirmed reconstruction**.

This document records the ordinary destruction/effect path at PPC `0x16300`,
the group/member consequence path at `0x36120`, and the outer inactive-member
cleanup pass around `0x36610` in the canonical Mac 1.0.6 executable. The clean
implementation is `src/core/destruction_runtime.cpp`.

The implementation deliberately keeps still-unresolved renderer/game-state behavior
behind explicit boundaries rather than inventing modern semantics. Ground-sensitive
spawn routing `0x16880`, the persistent obstacle Rect store, the ground-obstacle
zero-velocity/stationary consequence, and concrete player pickup/shield/death-entry
mutation are now recovered. Remaining boundaries include renderer/terrain pixel
mutation, the special live-member `+0xCD` destruction path, and the downstream player
life/respawn/game-over state machine.

## Binary anchors

| Routine | Recovered role |
| --- | --- |
| `0x14F10` | collision shield/damage path; ordinary lethal damage calls `0x16300` immediately |
| `0x16300` | ordinary destruction effects and random-bonus selection |
| `0x16880` | terrain-sensitive destruction/deletion-spawn routing; water replacement semantics recovered |
| `0x36120` | child cascade, group kill accounting, reward coins, destruction/removal |
| `0x363C0` | destroy children willing to die with owner |
| `0x364F0` | delete children willing to disappear with owner |
| `0x36610` | outer inactive-member cleanup; obstacle/owner/deletion-spawn handling |
| `0x36AB0` | safe pointer+serial+active reference validation |

## Unit Definition field anchors

The loader/parser correlation and runtime consumers establish:

| Source key | Compiled UnitDef offset | Use |
| --- | ---: | --- |
| `deletionSpawn_ID` | `+0x2DC` | spawn on deletion in outer cleanup |
| `destructSpawn_ID` | `+0x478` | destruction spawn |
| `destructParticle_ID` | `+0x47C` | destruction particle system |
| `destructParticleColor_COLOR` | `+0x480` | packed particle color |
| `destructNotice_STR` | `+0x482` | destruction notice text |
| `destructNumCoinsToRelease_INT` | `+0x4A4` | number of ordinary reward coins |
| `destructCoin_ID` | `+0x4A8` | ordinary reward coin Unit ID |
| `destructCoinOnGroupKill_ID` | `+0x4AC` | final-group-kill reward Unit ID |
| `destructDestroyChildren_BOOL` | `+0x4B0` | invoke owner-destruction child cascade |
| `destructDeleteChildren_BOOL` | `+0x4B1` | invoke owner-deletion child cascade |
| `destructCreateObstacle_BOOL` | `+0x4B2` | obstacle conversion during outer cleanup |
| `destructDrawToTerrain_BOOL` | `+0x4B3` | ground-unit destruction draw into terrain |
| `destructReleaseRandomBonus_BOOL` | `+0x4B4` | run random-bonus selector |
| `score_INT` | `+0x4B8` | score value consumed before ordinary lethal destruction |
| `destructSound_ID` | `+0x4BC` | destruction sound descriptor start |
| `destructSound_MinVolume_INT` | `+0x4C0` | sound descriptor |
| `destructSound_MaxVolume_INT` | `+0x4C4` | sound descriptor |
| `destructSound_Priority_INT` | `+0x4C8` | sound descriptor |
| `destructSound_MinPitch_FLOAT` | `+0x4CC` | sound descriptor |
| `destructSound_MaxPitch_FLOAT` | `+0x4D0` | sound descriptor |
| `pickup_Type_ID` | `+0x4D4` | player pickup category |
| `pickup_Value_INT` | `+0x4DC` | player pickup value |

State fields used by child/owner propagation are:

| Source key | Compiled state offset |
| --- | ---: |
| `canBeDestroyedOnOwnerDestruction_BOOL` | `+0x329` |
| `canBeDeletedOnOwnerDeletion_BOOL` | `+0x32A` |
| `passHitsToOwner_BOOL` | `+0x32B` |
| `destroyOwnerOnDestruction_BOOL` | `+0x32D` |

Canonical `Game.pak` uses 127 states willing to be destroyed with an owner and
138 states willing to be deleted with an owner. It currently uses zero states
with `destroyOwnerOnDestruction_BOOL`, but that executable-supported path is
retained in the clean model.

## Ordinary destruction — `0x16300`

The ordinary path is guarded by the live inactive/destruction byte so its
side-effects execute only once. For the regular path, recovered ordering is:

1. original housekeeping helper `0x12C00` (semantic name still unresolved);
2. if the entity is ground-domain and `destructDrawToTerrain_BOOL`, obtain its
   integer rect and request destruction draw-to-terrain;
3. if `destructParticle_ID != none`, emit destruction particles at the live
   x/y with `destructParticleColor_COLOR` and the ground/air fact;
4. if `destructSpawn_ID != none`, run recovered `0x16880` media routing; construct the requested spawn when allowed, otherwise optionally emit its water-impact replacement with the source entity's x/y, player owner, and self
   pointer+serial as parent;
5. if `destructNotice_STR` is non-empty, post the notice at the current tick;
6. if `destructSound_ID != none`, pass the complete sound descriptor to the
   sound helper;
7. mark the entity destroyed/inactive and preserve the supplied source owner;
8. if `destructReleaseRandomBonus_BOOL`, draw one inclusive integer `0..100`
   and run the random-bonus selector.

The clean trace preserves notice tick, particle color/ground fact, complete
sound descriptor, and spawn-request seed rather than reducing effects to only a
resource ID.

### Immediate collision/pickup ordering

Ordinary shield depletion in `0x14F10` calls `0x16300` at `0x15078` before
returning. Successful pickup collision calls `0x16300` at `0x34214`, then sets
live `+0xCA` at `0x3421C..0x34220`.

The clean collision API therefore accepts an optional destruction context and
trace. When supplied, lethal collision and successful pickup execute the
recovered destruction effects immediately so random-bonus RNG consumption stays
inside the original collision-loop order. Bounded callers that omit the
context still receive the proven lifecycle/source result and can finalize it
through the outer removal pass later.

## Random bonus — `0x16528..0x167B8`

The executable consumes fixed positional resources:

- `Game[gafl]` indices `209..219`;
- `Objects[gaob]` indices `25..34`.

The clean binder verifies the canonical labels before accepting those tables,
then applies PPC `fctiwz`-equivalent truncation to the float thresholds.
Canonical 1.0.6 values are:

```text
Game_RandomBonusPercent_1..9 = 70, 78, 82, 84, 87, 91, 95, 98, 100
Game_RandomBonusPercent_GroundAccuracyReward = 10
Game_MinimumLevelForHighestRandomBonus = 3
Objects[gaob] 25..34 = rb01, rb02, rb03, rb04, rb05,
                       rb06, rb07, rb08, rb09, rb10
```

Selection uses strict `<` comparisons after an inclusive `0..100` draw:

- `<70` -> `rb01`, except a pending ground-accuracy reward with roll `<10`
  selects `rb06` and clears the pending flag;
- `<78` -> `rb02`;
- `<82` -> `rb03`;
- `<84` -> `rb04`;
- `<87` -> `rb05`;
- `<91` -> `rb06`;
- `<95` -> `rb07`;
- `<98` -> `rb08`;
- `>=98` while progression `<3` -> `rb08`;
- otherwise `<100` -> `rb09`;
- `100` -> `rb10`.

The game-context value used for the progression test is the same `+0x14`
returned by `0x5CD0` and used by level-scaled shield initialization. Its final
high-level gameplay name is still intentionally unresolved.

## Group/member removal — `0x36120`

The normal group container fields are now semantically separated:

| Group offset | Meaning |
| ---: | --- |
| `+0xA4` | original member count |
| `+0xA8` | active member count |
| `+0xAC` | destroyed member count |

This distinction matters: **group kill is based on destroyed count reaching the
original member count**, not merely active count reaching zero. A group can
therefore become empty by deletion without being considered killed.

Recovered order for one member removal is:

1. if this is a destruction and the Unit Definition requests it, recursively
   destroy willing children;
2. independently, if requested, recursively delete willing children;
3. for destruction, increment group destroyed count and detect group kill on
   equality with original member count;
4. if destruction is player-attributed and live `+0xCA` is clear, emit the
   configured ordinary reward coin count and, on a final group kill, one
   configured group-kill coin;
5. run `0x16300` for a destruction (idempotent if effects already ran at the
   lethal collision/pickup site), otherwise keep the deletion lifecycle;
6. mark the member removed and decrement active-member count;
7. request group-container removal only when active count reaches zero and the
   group ID is not special `SERM`.

`+0xCA` is the pickup-consumed marker set immediately after successful pickup
`0x16300`; it suppresses the ordinary/group reward-coin branches so collecting
a pickup cannot recursively award destruction coins.

### `SERM` exemption

`SERM` is special in `0x36120`: destroyed-count accounting can still say its
final member completed a group kill, but it does not receive the ordinary
final-group-kill reward branch and active count reaching zero does not request
normal group-container removal.

## Child matching — `0x363C0` / `0x364F0`

Both child helpers scan the live-member lists. Their owner match is notably
weaker than a normal safe reference: they compare **only** candidate parent
serial (`live +0x144`) with owner live serial (`+0x9C`). They do not call
`0x36AB0` to validate the parent pointer.

A matched candidate is processed only when its current state opts in:

- destruction helper: `canBeDestroyedOnOwnerDestruction_BOOL`;
- deletion helper: `canBeDeletedOnOwnerDeletion_BOOL`.

The clean runtime preserves this serial-only matching behavior rather than
silently replacing it with the stricter normal safe-reference contract.

## Outer inactive-member cleanup — `0x36610`

The outer pass supplies consequences that are not inside `0x36120` itself:

1. if `destructCreateObstacle_BOOL`, request obstacle conversion for the
   inactive member;
2. for a destroyed member whose current state has
   `destroyOwnerOnDestruction_BOOL`, validate its parent safe reference and run
   ordinary destruction on that owner using the child's destruction source;
3. for a deleted (not destroyed) member with `deletionSpawn_ID != none`, apply
   recovered `0x16880` media routing and construct either the allowed deletion spawn or its water-impact replacement;
4. enter `0x36120` with destruction/player-attribution flags derived from the
   live member.

The clean implementation deliberately performs one forward insertion-order
traversal. If a child destroys a parent that lies later in traversal, that
parent is finalized later in the same pass. If the parent lies earlier, it is
picked up on a subsequent cleanup pass. This preserves the original list-order
property instead of recursively modernizing it.

## `0x16880` recovered media-routing boundary

The destruction/deletion helper is now reconstructed directly from the Mac 1.0.6 PPC path:

- non-ground units return allowed without a media lookup;
- `doDeathSpawnOnAnyMedia_BOOL` at UnitDef `+0x12B` also returns allowed;
- other ground units sample `(trunc(x)+32, trunc(y)+worldYOrigin)` through `0xFEE0`;
- `0xFEE0` returns true only for Media Mask cell value `31`, identified by the surrounding fixed Water-impact resource contract;
- non-water returns allowed;
- water suppresses the caller's requested destruction/deletion spawn and may emit a replacement chosen by `mediaImpactSize_ID` at UnitDef `+0x2E4`.

Fixed `Objects[gaob]` slots 6..9 are label-verified `spti/spsm/spme/spla` Water Tiny/Small/Medium/Large resources. The `tiny`, `smal`, `med `, `larg`, `smra`, `mera`, and `lara` selectors, including the original random branch order and RNG consumption, are implemented and regression-covered. See `TERRAIN_MEDIA_RUNTIME.md`.

The old generic `LegacyRemovalSpawnGate` placeholder has been removed; destruction and deletion spawns now pass through the concrete media decision core.

## Canonical corpus coverage

The strengthened `deimos_reference_probe` compares all newly compiled fields
back to the parsed `.unde` source and reports for canonical `Game.pak`:

- 99 Unit Definitions with destruction spawns;
- 99 with destruction particles;
- 77 with destruction sounds;
- 28 ordinary destruction-coin reward units;
- 15 group-kill coin reward units;
- 54 destroy-children units;
- 58 delete-children units;
- 13 obstacle-creation units;
- 32 draw-to-terrain units;
- 7 random-bonus units;
- zero canonical deletion-spawn units and zero canonical destruction notices;
- 127 states willing to be destroyed with owner;
- 138 states willing to be deleted with owner;
- zero canonical states that destroy their owner.

The absence of canonical use does not make an executable-supported path dead;
it simply gives those paths lower real-corpus coverage and keeps synthetic
regression tests important.

## Current clean boundary

Implemented and regression-covered:

- field compilation and source-key validation;
- ordinary effect ordering;
- complete destruction sound descriptor/notice tick facts;
- immediate collision/pickup `0x16300` ordering when context is supplied;
- random-bonus resource binding and exact threshold chain;
- child destruction/deletion recursion;
- destroyed/active/original group counters;
- pickup reward suppression;
- `SERM` behavior;
- obstacle/deletion-spawn/owner-destruction outer cleanup facts;
- exact `0x16880` water-media routing and replacement-spawn behavior;
- persistent ground-obstacle Rect append/shift/inclusive-overlap/reset semantics;
- single-pass list-order cleanup semantics.

Still separate or unresolved:

- integrate the proven zero-velocity/stationary ground-obstacle consequence into the member tick around the still-bounded live `+0x19` gate;
- renderer/terrain bitmap mutation beyond the recovered persistent Rect store and obstacle trace;
- special live `+0xCD` destruction path through `0x17E70`;
- a few early/late `0x16300` bookkeeping calls whose global semantics are not
  yet named;
- downstream player life decrement/respawn/game-over logic and full orchestration of the now-concrete pickup/shield/death-entry routines;
- integration of every non-collision destruction entry site into one full game
  tick orchestration.
