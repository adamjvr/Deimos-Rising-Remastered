# Mac 1.0.6 terrain/media runtime notes

Status: **binary-confirmed** for the `0x16880` media decision core and
`0x2A6D0/0x2A770/0x2A830/0x2A950` obstacle-rectangle list.

## Relevant entry points

| PPC address | Recovered role |
| ---: | --- |
| `0x16880` | ground-sensitive destruction/deletion spawn routing and water-impact replacement |
| `0xFEC0` | supplies the current world/background Y origin used by the sample point |
| `0xFEE0` | Media Mask sample; true iff the in-bounds 16-bit cell equals `31` |
| `0x12A00` | build live-member QuickDraw Rect via independent `fctiwz` edge conversion |
| `0x2A6D0` | append one persistent ground-obstacle/background Rect |
| `0x2A770` | shift all stored Rect top/bottom values by vertical scroll delta |
| `0x2A830` | inclusive rectangle overlap query |
| `0x2A950` | free/clear stored Rect list |
| `0x12F20` | refresh/rebuild live render record during obstacle conversion; internals not yet named |

## `0x16880` control flow

`UnitDef+0x125` (`isGroundBased_BOOL`) is the first branch. Air units return
allowed. Ground units then test `UnitDef+0x12B`, directly mapped by the parser to
`doDeathSpawnOnAnyMedia_BOOL`; true also returns allowed.

The remaining path obtains live x/y, applies independent PPC truncation, and
samples the Media Mask at `(trunc(x)+32, trunc(y)+worldYOrigin)`. A false
`0xFEE0` result returns allowed. A true result enters a switch on
`UnitDef+0x2E4` (`mediaImpactSize_ID`) and ultimately returns false, thereby
suppressing the destruction/deletion spawn requested by the caller.

The switch may construct a replacement water impact itself. Fixed object slots
are `Objects[gaob]` 6..9:

```text
6 MediaImpact_Water_Tiny   spti
7 MediaImpact_Water_Small  spsm
8 MediaImpact_Water_Medium spme
9 MediaImpact_Water_Large  spla
```

Switch IDs:

```text
tiny -> 6
smal -> 7
med  -> 8
larg -> 9
smra -> rng(0..1): 0->7, 1->6
mera -> rng(0..2): 0->6, 1->7, 2->8
lara -> rng(0..1): 0->9, 1->8
```

Unknown/none size IDs still suppress the caller's spawn on a water sample but
emit no replacement. The random branches consume the shared legacy RNG only
after a water hit.

## `0xFEE0`

The helper obtains Media Mask dimensions/data, converts the input coordinates
through the mask element scale, bounds-checks, indexes a 16-bit cell, and
returns one only for value `31`. The water-impact resource switch immediately
above provides the semantic evidence for treating that value as the water mask
class in this path.

## Ground-obstacle rectangle list

`0x2A6D0` allocates 16 bytes and copies a QuickDraw Rect
`{top,left,bottom,right}` into an append-only list. `0x2A830` rejects only strict
separation:

```text
input.bottom < obstacle.top
input.top    > obstacle.bottom
input.right  < obstacle.left
input.left   > obstacle.right
```

Therefore edge contact counts as collision. `0x2A770` shifts only top/bottom;
`0x2A950` clears the list.

The main member update checks `UnitDef+0x128`, parser-mapped to
`collidesWithGroundObstacles_BOOL`, before calling this overlap helper. A hit
returns the member toward its pre-move position and sets a live latch byte; the
clean core currently models the exact query but leaves that larger tick
orchestration for the next boundary.

`destructDrawToTerrain_BOOL` calls `0x2A6D0` with the entity Rect during
`0x16300`, tying destruction terrain draw requests to the same later collision
store.

## Obstacle conversion

When `destructCreateObstacle_BOOL` is processed by outer cleanup, the executable
sets live render/obstacle bytes, copies UnitDef `+0x11E` (`castsShadows_BOOL`),
and calls `0x12F20`. The clean trace retains the entity Rect plus shadow flag.
The precise renderer-record rebuild/pixel composition behind `0x12F20` remains
outside this recovered subset.
