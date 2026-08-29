# Particle Lifecycle and Gameplay Producers

This checkpoint closes the producer/update side of the Mac 1.0.6 particle
subsystem that was left open after the exact `0x43BA0` raster reconstruction.
The clean core now models the complete binary-confirmed chain:

```text
0x44630 / 0x431F0  startup direction tables + cursors
          |
          v
0x43340             particle-system construction
          |
          v
0x438C0             update / fade / bounds / prune
          |
          v
0x43BA0             exact 7x7 xRGB1555 raster
```

The three recovered gameplay producers now also build the exact 24-byte spawn
request and can execute `0x43340` inline at the original RNG position:

- state particles in the member tick (`0x33A7C..0x33B60`);
- non-lethal collision-hit particles (`0x150BC..0x15114`);
- destruction particles (`0x1636C..0x163C4`).

A nullable `LegacyParticleExecutionContext` keeps isolated/headless tests from
manufacturing global particle state. When supplied, construction happens inline
and therefore consumes the shared legacy RNG exactly where the original call did.

## Spawn request (`0x43340`)

All three direct callers construct the same semantic request:

| Offset | Type | Meaning |
| ---: | --- | --- |
| `+0x00` | float | world/screen X |
| `+0x04` | float | world/screen Y |
| `+0x08` | uint16 | xRGB1555 source color |
| `+0x0C` | int32 | system delay |
| `+0x10` | byte | ground-space tracking flag |
| `+0x14` | FourCC | particle preset |

The gameplay producers recovered so far always pass delay zero. The ground-space
flag is true exactly when the owning Unit Definition's compiled media/collision
domain is `grnd`. During `0x438C0`, that flag causes the particle Y coordinate to
receive `0xFED0`'s **applied terrain vertical-scroll delta** before velocity
integration. It is not a generic gravity flag.

Source RGB24 colors are packed as xRGB1555 by dropping the low three bits of each
channel.

## Accepted preset IDs

`0x43340` accepts exactly eight IDs:

| Preset | Count | Velocity magnitude | Direction table |
| --- | ---: | ---: | --- |
| `tiny` | 5 | 3 | varied |
| `smal` | 10 | 3 | varied |
| `med ` | 20 | 5 | varied |
| `larg` | 40 | 5 | varied |
| `tici` | 5 | 3 | circular/unit |
| `smci` | 10 | 3 | circular/unit |
| `meci` | 20 | 5 | circular/unit |
| `laci` | 40 | 5 | circular/unit |

Zero, `none`, and unrecognized IDs return without allocating a system and without
consuming RNG.

The system object is `0x470` bytes in the original. Recovered trailing fields are:

| Offset | Meaning |
| ---: | --- |
| `+0x464` | ground-space flag |
| `+0x465` | reverse-blend flag |
| `+0x466` | short-velocity flag |
| `+0x467` | circular/unit-direction flag |
| `+0x468` | delay countdown |
| `+0x46C` | particle count |

Every recovered producer initializes reverse blend to false. The alternate
reverse-fade branch is retained because `0x438C0` contains executable support for
it even though no recovered direct producer sets it.

## Startup direction tables (`0x44630` / `0x431F0`)

Startup generates two parallel 100-entry float-vector tables. For every slot:

1. choose random integer X in `[0, visibleWidth]`;
2. choose random integer Y in `[0, visibleHeight]`;
3. subtract the half-width/half-height center;
4. compute `sqrt(fctiwz(dx*dx + dy*dy))` in the legacy single-precision path;
5. normalize the vector;
6. store it unchanged in the circular table;
7. choose one of four speed factors and store the scaled copy in the varied table.

The varied factors are exactly `1.00`, `0.85`, `0.70`, and `0.55`. The circular
copy always remains unit length. Two additional `[0,99]` RNG draws seed the two
cursors, so ordinary non-degenerate startup consumes **302 RNG draws**.

The cursor increment has a legacy quirk: after use it increments and resets to
zero when the result is `>=99`. Normal cycling therefore visits `0..98`; entry
99 can be used only when startup initially seeds a cursor to 99.

The synthetic regression freezes an independent oracle for seed `0x12345678` at
416x480:

```text
final seed       = 0x3af362fa
varied cursor    = 27
circular cursor  = 91
circular[0]      = (0.8320502639, 0.5547001958)
varied[0]        = (0.7072427273, 0.4714951515)
```

## Color variants

Construction expands the source 5-bit color channels through the executable's
16-bit intermediate path, then builds five brightness variants using
`1 - variant * Particle_ColorVariationAdjust` for variant `0..4`. Fringe colors
apply `Particle_FringeColorAdjust` after the variant scale. Each emitted particle
consumes one inclusive random draw in `[0,4]` to select its core/fringe pair.

With canonical tuning this uses:

```text
Particle_ColorVariationAdjust = 0.12
Particle_FringeColorAdjust    = 0.6
```

Velocity scale is 3 for short presets and 5 for long presets.

## Update / prune (`0x438C0`)

For each particle system the routine first decrements `+0x468`. A result greater
than zero skips the entire system for that tick. Otherwise each active particle:

1. optionally adds the applied terrain-scroll delta to Y for ground-space systems;
2. multiplies **both** velocity axes by `Particle_Gravity` (`0.96` canonically);
3. integrates X/Y;
4. applies the footprint bounds;
5. advances the blend lifetime;
6. is removed when its lifetime/bounds branch says inactive.

The normal bounds are inclusive at their upper footprint edge:

```text
x >= -32
x + 7 <= visibleWidth + 32
y >= 0
y + 7 <= visibleHeight
```

For ordinary forward blend, out-of-bounds particles deactivate immediately.
While blend `<32`, `Particle_BlendAmountRate_Long` is added and clamped to 32.
A particle already at 32 at the start of a subsequent update deactivates.
Canonical long rate is 1.

The executable reverse-blend branch ignores the normal out-of-bounds kill,
subtracts the same long rate toward zero, and deactivates on the update after it
is already zero. `Particle_BlendAmountRate_Short` is loaded as canonical tuning
but is not used by this `0x438C0` path.

A system is released when no active particles remain.

## State-particle producer (`0x33A7C..0x33B60`)

Compiled state fields are now bound as:

```text
+0x2D0  stateParticles_ID
+0x2D4  stateParticlesColor_COLOR (packed xRGB1555 in executable)
+0x2D6  stateParticlesRepeat_BOOL
+0x2D8  stateParticles_RepeatDelay_INT
+0x2DC  stateParticles_MaxNumBursts_INT
```

Live-member producer bookkeeping is:

```text
+0xF0  last particle-burst tick
+0xF4  particle-burst count
```

The gate is exact:

- no/`none` particle ID -> skip;
- repeat=true: a zero last-tick is immediately due; otherwise due when
  `currentTick >= lastTick + repeatDelay` using the original 32-bit arithmetic;
- repeat=false: due only while burst count is zero;
- nonzero max-bursts additionally requires `burstCount < maxBursts`.

After a due attempt, the burst count increments and last-burst tick becomes the
current tick **even if `0x43340` rejects an unknown preset**.

State entry has an important quirk: it resets only live `+0xF4` burst count. It
does **not** reset `+0xF0` last-burst tick. Fresh member construction zeros both.
The clean regression freezes this behavior.

This producer executes before the timer/rule portion of the member tick. The
original outer member path decrements/tests the separate group-delay field before
reaching this block; the bounded `advance_entity_runtime()` helper continues to
assume that outer gate has already been handled by its caller.

Canonical `Game.pak` contains **7 states with a non-none state-particle ID; 4 of
those states have repeat enabled**.

## Collision-hit producer (`0x150BC..0x15114`)

On the non-lethal damage path, a non-none `hitParticles_ID` constructs a request
using:

```text
x/y       = target live-member x/y
color     = UnitDef hitParticlesColor_COLOR
preset    = UnitDef hitParticles_ID
delay     = 0
ground    = target UnitDef domain == grnd
```

The recovered direct call does not separately consume
`hitParticleDoCircularBurst_BOOL`; the preset ID itself is what reaches
`0x43340`. The source Boolean is still compiled/preserved as evidence. Canonical
1.0.6 has **3 hit-particle units and zero with that circular flag enabled**, so no
unsupported behavior is invented for a source combination the shipped corpus
does not exercise.

## Destruction producer (`0x1636C..0x163C4`)

Destruction effects invoke `0x43340` in their existing consequence order, after
terrain mutation and before destruction spawn/notice/sound/random-bonus work.
The request is:

```text
x/y       = destroyed member x/y
color     = destructParticleColor_COLOR
preset    = destructParticle_ID
delay     = 0
ground    = UnitDef domain == grnd
```

When a particle execution context is installed, particle color-variant RNG is
therefore consumed before later destruction RNG exactly as in the executable.
Canonical 1.0.6 has **99 destruction-particle Unit Definitions**.

## Validation

The repository synthetic suite is now **37/37 PASS in Debug**. New regression
coverage includes:

- 302-draw startup direction/cursor oracle;
- preset count/velocity/table mapping and cursor-99 quirk;
- no-RNG unknown-preset behavior;
- RGB24 -> xRGB1555 request packing;
- delay, ground-scroll, damping, integration, inclusive bounds and fade lifetime;
- state one-shot/repeat/max-burst gates and state-entry timestamp quirk;
- exact inline producer RNG execution;
- collision-hit and destruction request construction.

The canonical `Game.pak` probe now independently compares every newly compiled
hit/state particle field against parsed source data and reports:

```text
Hit-particle units: 3 (circular flag=0)
State-particle states: 7 (repeat=4)
destruction particle effects: 99
```

Existing deterministic render/gameplay oracles remain unchanged:

```text
sprite-surface FNV64      0x9f9dcfba05b5089c
software-render FNV64     0x32290b39b091e970
terrain viewport/depth    416x480x16
groups / members          386 / 546
active after first tick   544
construction RNG seed     2249411936
motion RNG seed           2633739833
```

## Next boundary

The clean particle subsystem is no longer the open frame-composition boundary.
The next renderer task is to continue outward from the closed `0x30BC0` world
composition segment and identify the original **native presentation ownership,
window/display copy/swap timing, and any player/UI overlays that sit outside that
segment**. Other Phase-1 gaps (special constructor/list-pool path, replay bits,
remaining Flee edges) stay independent.
