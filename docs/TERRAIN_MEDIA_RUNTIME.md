# Terrain / media destruction runtime — Mac 1.0.6

Status: **binary-confirmed clean subset**.

This milestone closes the destruction/deletion media helper at PPC `0x16880`
and reconstructs the persistent rectangle store used by ground-obstacle
collision (`0x2A6D0`, `0x2A770`, `0x2A830`, `0x2A950`). It deliberately does
not claim that terrain pixels or renderer records are fully reconstructed.

## Direct Unit Definition anchors

Parser-key/destination correlation establishes:

| Source key | Compiled UnitDef offset | Runtime use |
| --- | ---: | --- |
| `castsShadows_BOOL` | `+0x11E` | copied into the live render state when a destroyed entity becomes an obstacle |
| `isGroundBased_BOOL` | `+0x125` | selects ground vs air behavior and derived collision domain |
| `collidesWithGroundObstacles_BOOL` | `+0x128` | gates the persistent obstacle-rectangle query |
| `doDeathSpawnOnAnyMedia_BOOL` | `+0x12B` | bypasses the media-mask replacement path in `0x16880` |
| `mediaImpactSize_ID` | `+0x2E4` | chooses the water-impact replacement family |

Canonical 1.0.6 coverage is 67 shadow-casting units, 4 ground-obstacle
colliders, 12 units with `doDeathSpawnOnAnyMedia`, and 3 units with a non-`none`
`mediaImpactSize_ID`.

## `0x16880`: death/deletion spawn media routing

The helper is more than a boolean spawn gate.

1. Non-ground units immediately allow the requested destruction/deletion spawn.
2. Ground units with `doDeathSpawnOnAnyMedia_BOOL` also allow it without a
   media-mask lookup.
3. Otherwise the helper samples at:
   - `x = trunc(entity.x) + 32`;
   - `y = trunc(entity.y) + worldYOrigin`.
4. `0xFEE0` reads the 16-bit Media Mask and returns true only when the sampled
   cell equals `31`.
5. A non-water sample allows the caller's requested spawn.
6. A water sample always suppresses the requested spawn. Depending on
   `mediaImpactSize_ID`, `0x16880` may itself construct a replacement water
   impact at the original entity position/ownership chain.

The clean runtime exposes this as `resolve_legacy_removal_media` and records
whether water was hit, whether the caller's original spawn remains allowed,
the sampled coordinates, any replacement FourCC, and the exact number of RNG
draws.

### Media size IDs

The recovered switch is:

| `mediaImpactSize_ID` | Replacement |
| --- | --- |
| `tiny` | Water Tiny |
| `smal` | Water Small |
| `med ` | Water Medium |
| `larg` | Water Large |
| `smra` | one 0..1 draw: 0 -> Small, 1 -> Tiny |
| `mera` | one 0..2 draw: Tiny / Small / Medium |
| `lara` | one 0..1 draw: 0 -> Large, 1 -> Medium |
| `none`, empty, unknown | no replacement, but the original spawn is still suppressed on water |

The apparently reversed `smra` and `lara` orders are preserved exactly because
they are executable behavior and affect both resource choice and RNG sequence.

Canonical `Game.pak` uses non-`none` media sizes only for:

- Tank Tracks (`tatr`) — `smal`;
- Bomb Crater (`bocr`) — `smra`;
- Plasma Bomb (`plbo`) — `smra`.

## Fixed water-impact resource contract

PPC `0x16880` consumes `Objects[gaob]` positions 6..9. The clean binder verifies
labels before accepting the table:

| Index | Required label | Canonical ID |
| ---: | --- | --- |
| 6 | `MediaImpact_Water_Tiny` | `spti` |
| 7 | `MediaImpact_Water_Small` | `spsm` |
| 8 | `MediaImpact_Water_Medium` | `spme` |
| 9 | `MediaImpact_Water_Large` | `spla` |

This surrounding resource contract is what identifies Media Mask value `31` as
the water path rather than an arbitrary material class.

## Persistent ground-obstacle rectangles

The background/terrain module owns an append-only list of 16-byte QuickDraw
Rects:

- `0x2A6D0` allocates/appends one rect without merging or de-duplication;
- `0x2A770` adds the vertical scroll delta to each rect's top and bottom;
- `0x2A830` reports overlap using inclusive edges (touching counts);
- `0x2A950` frees/clears the list on teardown/reset.

The main member update checks `collidesWithGroundObstacles_BOOL`, derives the
entity rect through `0x12A00`, and queries `0x2A830`. The clean core currently
implements the exact rect conversion, flag gate, list persistence, vertical
shift, inclusive overlap, and clear semantics. The subsequent original
position rollback/latch orchestration remains a separate tick-integration
boundary.

`destructDrawToTerrain_BOOL` also appends the destroyed ground entity's rect to
this same list. This proves the list participates in later ground-obstacle
collision; it should not be described merely as a visual invalidation list.

## `destructCreateObstacle_BOOL`

Outer inactive-member cleanup performs a distinct conversion path:

- live obstacle/render byte is enabled;
- another live render byte is cleared;
- UnitDef `castsShadows_BOOL` is copied into the live render state;
- `0x12F20` rebuilds/refreshes the live render record before normal member
  teardown continues.

The clean destruction trace now captures the obstacle rect and `castsShadows`
fact. Exact renderer-record mutation and any underlying bitmap composition are
not yet claimed as reconstructed.

## Validation

The repository suite is **23/23 PASS**. The canonical `Game.pak` probe verifies
all new compiled fields and the fixed water-impact labels/IDs while retaining
the established deterministic baseline:

- 386 groups / 546 live members after construction;
- construction RNG seed `2249411936`;
- 544 active after the first player-aware tick;
- motion RNG seed `2633739833`.

## Remaining terrain-facing boundaries

- integrate the original ground-obstacle hit rollback/latch into the complete
  member tick;
- reconstruct the renderer/bitmap consequences behind `0x12F20` and any actual
  terrain-image mutation beyond the proven rectangle store;
- bind a decoded Media Mask resource/provider instead of the clean callback
  boundary;
- recover the special live `+0xCD -> 0x17E70` destruction route and remaining
  destruction entry-site orchestration.
