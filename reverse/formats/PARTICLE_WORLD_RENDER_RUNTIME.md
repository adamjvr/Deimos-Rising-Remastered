# Particle Raster and World-Frame Runtime

This checkpoint closes the remaining recovered composition slot around PowerPC
`0x30BC0` and identifies `0x43BA0` precisely. The latter is **not** native
presentation or a generic post-process: it is the direct 16-bit particle
rasterizer for the original game's particle subsystem.

The clean implementation is split into:

- `include/deimos/particle_runtime.hpp`
- `src/core/particle_runtime.cpp`
- `include/deimos/world_render_runtime.hpp`
- `src/core/world_render_runtime.cpp`
- `tests/particle_runtime_test.cpp`
- `tests/world_render_runtime_test.cpp`

Native DrawSprocket/window/GPU presentation remains intentionally outside this
portable composition boundary.

## Recovered executable cluster

Direct disassembly of the recovered Mac 1.0.6 PPC application bounds a coherent
particle subsystem:

| PPC routine | Recovered role |
| --- | --- |
| `0x431F0` | particle subsystem initialization |
| `0x432D0` | particle list/container allocation |
| `0x43340` | particle-system spawn/construction |
| `0x438C0` | particle update/prune |
| `0x43BA0` | visible-surface 16-bit particle raster |
| `0x44550` | particle list clear/destruction |
| `0x44630` | initialization of 100 random direction vectors |
| `0x44840` | individual system release |
| `0x46580` | random integer helper used by the cluster |

Canonical Unit Definitions reference this subsystem through fields such as
`destructParticle_ID` and `stateParticles_ID`.

## Particle tuning table

`0x43340/0x438C0` use the following strict `Game[gafl]` positional contract:

| 0-based index | Canonical label | Canonical value |
| ---: | --- | ---: |
| 144 | `Particle_Gravity` | `0.96` |
| 145 | `Particle_ColorVariationAdjust` | `0.12` |
| 146 | `Particle_FringeColorAdjust` | `0.6` |
| 147 | `Particle_BlendAmountRate_Short` | `3.0` |
| 148 | `Particle_BlendAmountRate_Long` | `1.0` |

Despite its legacy name, `Particle_Gravity` is multiplied into both velocity
components by `0x438C0`; in the recovered update path it therefore behaves as a
velocity damping factor.

`compile_legacy_particle_tuning()` validates both the labels and positional
values contract instead of silently accepting a shifted table.

## Particle record layout

The rasterizer consumes 28-byte particle records. The fields required by the
recovered boundary are:

| Offset | Width | Meaning |
| ---: | ---: | --- |
| `+0x00` | 1 | active byte |
| `+0x02` | 2 | xRGB1555 core color |
| `+0x04` | 2 | xRGB1555 fringe color |
| `+0x08` | 4 | blend/transparency amount `q` |
| `+0x0C` | 4 | float X |
| `+0x10` | 4 | float Y |
| `+0x14` | 4 | float X velocity, updated by `0x438C0` |
| `+0x18` | 4 | float Y velocity, updated by `0x438C0` |

A particle-system object carries a positive countdown at `+0x468`; `0x43BA0`
skips the whole object while that value is positive. The count used by the
legacy object is at `+0x46C`.

## Exact screen-space clipping

`0x43BA0` subtracts the horizontal view offset returned by `0x100A0` from
particle X. Particle Y is already in visible-game coordinates at this boundary.
The executable then performs float tests before integer conversion:

```text
screenX >= 0
screenX + 7 < VisibleGameWidth
screenY >= 0
screenY + 7 < VisibleGameHeight
```

The constants `0.0` and `7.0` are recovered directly from the PEF constant
data. Only after all four tests pass does the code convert X/Y with PPC
truncate-toward-zero semantics and emit the complete 7x7 kernel without
per-pixel clipping.

## Exact 7x7 raster kernel

The particle renderer is an unrolled radial xRGB1555 blend. Let `q` be the
particle's blend amount. The four transparency levels are:

```text
W22    = min(q + 22, 31)
W10    = min(q + 10, 31)
W6     = min(q +  6, 31)
Q      = q
CENTER = (q > 6) ? (q - 7) : q
```

The transparency matrix is:

```text
W22 W22 W10 W10 W10 W22 W22
W22 W10 W6  W6  W6  W10 W22
W10 W6  Q   Q   Q   W6  W10
W10 W6  Q   CENTER Q  W6  W10
W10 W6  Q   Q   Q   W6  W10
W22 W10 W6  W6  W6  W10 W22
W22 W22 W10 W10 W10 W22 W22
```

The core color at record `+0x02` is used in exactly a five-pixel plus shape:

```text
. . . C . . .
. . C C C . .
. . . C . . .
```

centered on rows/columns 2..4 of the 7x7 footprint. Every other tap uses the
fringe color at `+0x04`.

Each tap reuses the already recovered xRGB1555 arithmetic:

```text
dst = (dst * transparency + src * (32 - transparency)) >> 5
```

The center expression contains a real legacy discontinuity: at `q == 7`,
`CENTER` becomes zero and the center returns to fully source-colored. The clean
regression preserves this rather than smoothing it into a more conventional
formula.

## Exact outer world-composition order

Direct `0x30BC0` call-site disassembly resolves the previous open boundary:

```text
0x18B20(group 0)     # layers 0..1; includes one-shot terrain writes
    ->
0x10120              # full 416x480 persistent-terrain viewport copy
    ->
0x18B20(group 1)     # layers 2..5
    ->
0x43BA0              # direct particle raster into visible surface
    ->
0x18B20(group 2)     # layers 6..15
```

The caller's draw-enabled latch in PPC register `r28` gates only `0x10120` and
`0x43BA0`. All three `0x18B20` queue-group flushes execute regardless of that
latch. `render_legacy_world_frame()` preserves this exact distinction.

This ordering matters because it gives direct pixel-level precedence:

- group-0 terrain mutations exist before the terrain viewport is copied;
- group-1 sprite pixels may be overwritten by particles;
- group-2 sprite pixels may overwrite particles.

`world_render_runtime_test` freezes all three precedence relationships and the
`r28` gate behavior.

## Validation

The raster/world-frame checkpoint originally established 36 passing tests; the current repository suite is **37/37 PASS in Debug** after the lifecycle/producer follow-up.
The canonical `Game.pak` probe additionally label-validates the five particle
tuning entries and reports:

```text
particle tuning: damping=0.96 colorVar=0.12 fringe=0.6 blendRates=3/1
```

All existing canonical gameplay/render oracles remain unchanged, including:

- 386 groups;
- 546 constructed live members;
- 544 active members after the first tick;
- construction RNG seed `2249411936`;
- motion RNG seed `2633739833`;
- sprite-surface FNV64 `0x9f9dcfba05b5089c`;
- software-render FNV64 `0x32290b39b091e970`;
- terrain viewport/depth `416x480x16`.

## Lifecycle follow-up / remaining presentation work

`PARTICLE_LIFECYCLE_RUNTIME.md` now closes `0x43340/0x438C0`, startup direction
tables, and the state/hit/destruction producers. The remaining renderer boundary
is therefore outside this composition segment: identify native presentation
ownership/timing and any UI/player overlays beyond `0x30BC0`, while preserving
the software compositor and particle-raster oracles.
