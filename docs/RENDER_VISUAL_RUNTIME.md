# Visual State and Render-Request Runtime — Mac 1.0.6

Status: **deterministic clean boundary reconstructed; legacy pixel backend still open**.

This milestone reconstructs the gameplay-visible state that feeds Deimos
Rising's sprite renderer without claiming that the original QuickDraw/image
submission backend has been cloned. The clean core now carries the same
visibility, tint, scale, layer, shadow, and terrain-draw facts that the PPC
engine produced, can emit ordered render intents, can construct the source
16-bit frame surfaces, and can compute the exact shadow request geometry for a
future native renderer.

## Recovered sprite base

Entities and players begin with the same 0x94-byte legacy sprite base. Important
live fields are now mapped as:

| Live offset | Meaning |
| ---: | --- |
| `+0x00/+0x04` | world position x/y |
| `+0x10/+0x14` | velocity x/y |
| `+0x18` | world-space transform gate; HUD entities clear it |
| `+0x19` | entity air-domain cache |
| `+0x1A` | adjust-shadow-location-for-scaling flag |
| `+0x1C/+0x20` | current sprite face / frame |
| `+0x24/+0x28` | scaled sprite dimensions |
| `+0x2C/+0x30` | collision/render half extents |
| `+0x34` | geometry/bounds dirty |
| `+0x36` | current state draw-to-terrain flag |
| `+0x37/+0x38` | temporary main/shadow pass selectors |
| `+0x4C` | draw-layer FourCC |
| `+0x50` | cached sprite/frame handle |
| `+0x54` | `stateDoColorise_BOOL` |
| `+0x58/+0x5C/+0x60` | tint current / required / delta |
| `+0x64` | tint color |
| `+0x68/+0x6C/+0x70` | visibility current / required / delta |
| `+0x74/+0x78/+0x80` | collision-glow active / amount / color |
| `+0x84/+0x88/+0x8C` | scale current / required / delta |
| `+0x90` | terrain submission sequence/cache boundary |

`+0x37/+0x38` are not persistent semantic properties. The world renderer
temporarily changes them to call `0x12F20` in shadow-only and main-only sorted
passes, then restores them.

## Unit Definition visual defaults

Direct parser/runtime correlation establishes:

- `initialScalePercent_INT` -> compiled UnitDef `+0x1AC`;
- `initialScalePercentTolerance_INT` -> `+0x1B0`;
- `initialVisibilityPercent_INT` -> `+0x1B4`;
- `drawLayer_ID` -> `+0x2E0`;
- `castsShadows_BOOL` -> `+0x11E`;
- `adjustShadowLocForScaling_BOOL` -> `+0x12C`.

Initial visibility remains a percentage-domain float. Initial scale is converted
to a scale factor. When scale tolerance is nonzero, the engine takes signed
`tolerance / 2`, draws an inclusive integer in `[-half,+half]`, adds it to the
base percentage, clamps a negative result to zero, and then converts to scale.
The clean implementation uses the existing PPC-compatible shared RNG helper, so
this draw participates in deterministic RNG ordering.

## State visual fields

The state parser and state-entry/update paths bind:

| Serialized state key | Compiled offset |
| --- | ---: |
| `stateSpriteFace_ID` | `+0x304` |
| `stateSpriteFrameMin_INT` | `+0x30C` |
| `stateSpriteFrameMax_INT` | `+0x310` |
| `stateUseParentDirection_BOOL` | `+0x324` |
| `stateTintColor_COLOR` | `+0x332` |
| `stateDoColorise_BOOL` | `+0x34D` |
| `stateDrawToTerrain_BOOL` | `+0x353` |
| `stateRequiredScalePercent_INT` | `+0x3BC` |
| `stateScaleDeltaPercent_INT` | `+0x3C0` |
| `stateRequiredVisibilityPercent_INT` | `+0x3C4` |
| `stateVisibilityDeltaPercent_INT` | `+0x3C8` |
| `stateTintPercent_INT` | `+0x3CC` |
| `stateTintDeltaPercent_INT` | `+0x3D0` |

Initial visual setup seeds current visibility from the Unit Definition and
current tint from the first state's tint. Ordinary later state changes replace
required values/deltas while preserving current visibility/tint/scale, allowing
the original per-tick ramps to converge naturally.

## Exact scalar ramps

PPC `0x12750` updates visibility and tint. PPC `0x12840` updates scale.

For a current value below its target the engine adds the configured delta and
clamps overshoot to the target. For a current value above its target it
subtracts the delta and clamps overshoot to the target. Visibility and tint
also clamp the decreasing side at zero. Scale deliberately does **not** have
that extra zero clamp.

Any actual scale change marks live `+0x34` dirty. Merely changing the target
scale does not. Sprite face/frame changes also invalidate geometry.

`0x12940` later rebuilds scaled dimensions and half extents from the current
sprite/frame and current scale. That path is now wired to the reconstructed
`0x19AD0/0x19C10/0x19CA0` sprite cache and dimension helpers: lazy resource
lookup, high-frame fallback to frame zero, and PPC `fctiwz` scaled dimensions
are preserved. A `none` face yields zero half extents while deliberately leaving
the prior width/height values stale, matching the original routine. See
`SPRITE_RESOURCE_RUNTIME.md` for the exact cache and atlas contract.

## Main draw layers

For non-terrain main sprite submissions, `0x12FA0` maps draw-layer FourCCs to
numeric renderer layers:

| Draw layer | Main layer |
| --- | ---: |
| zero / `none` / `defa`, ground | 3 |
| zero / `none` / `defa`, air | 7 |
| `grou` | 3 |
| `grhi` | 5 |
| `ailo` | 7 |
| `aihi` | 8 |
| `plwe` | 9 |
| `play` | 10 |
| `plsh` | 11 |
| `plef` | 12 |
| `plui` | 13 |
| `atmo` | 14 |
| `hud ` | 15 |

Zero and `none` are converted to `defa` by the original routine before the
switch. The earlier research transcription `mowe/moay` was corrected by
re-reading the PPC literal construction: the actual FourCCs are `plwe/play`.
Canonical `Game.pak` uses `plwe` on five Unit Definitions.

## Shadow layers and exact transform — `0x13460`

`0x13460` uses a companion layer domain:

```text
default ground / grou -> 2
grhi                  -> 4
default air            -> 6
air/player/HUD layers  -> 6
terrain submission     -> 0
```

The transform itself is now recovered and lives in `SHADOW_RUNTIME.md`. Canonical `Game[gafl]` positions 48..51 are label-verified as `Shadow_XOffset=-48`, `Shadow_YOffset=104`, `Shadow_GroundXOffset=-6`, and `Shadow_GroundYOffset=8`. Air shadows render at `0.5 * entityScale`; ground shadows render at `entityScale`. The `adjustShadowLocForScaling_BOOL` branch controls whether air offsets scale with the entity or remain on the fixed 0.5 basis.

Ordinary world-space requests apply the bounded horizontal view offset from `0x100A0`. Main terrain stamps instead use `trunc(worldX) + 32` with the world/background Y origin from `0xFEC0`; terrain shadows remain a separate `0x13460` path and retain their recovered `-32` terrain X basis. Visibility is converted through the original 0–32 transparency mapping and then clamped to a minimum transparency value of 20.

## Render intent order

`0x12F20` exits immediately for visibility `<= 0`. For an eligible entity it
then orders work as:

1. shadow request, when the selected shadow pass and global shadow setting allow;
2. main sprite request path.

Inside `0x12FA0` the main path is:

1. ordinary base sprite only when `stateDoColorise_BOOL == false`;
2. tint effect when current tint is greater than zero;
3. collision-glow effect when the glow-active byte is set.

Thus `stateDoColorise_BOOL` does **not** mean "draw a tint on top of the normal
sprite". It suppresses the normal base submission; tint and glow remain
independent effect requests.

`stateDrawToTerrain_BOOL` bypasses the ordinary main-layer switch and enters a
separate terrain submission/sequence path involving live `+0x90`. The clean render intent preserves that distinction; subsequent backend reconstruction proves main terrain submissions use one-shot layer 1 and terrain shadows use one-shot layer 0.

## Canonical corpus

The Mac 1.0.6 `Game.pak` probe validates every newly compiled visual field
against its parsed `.unde` source:

- 17 Unit Definitions use nonzero initial scale tolerance;
- 0 stock Unit Definitions enable `adjustShadowLocForScaling_BOOL`;
- 2 states enable `stateDrawToTerrain_BOOL`;
- 62 states enable `stateDoColorise_BOOL`;
- 111 states have nonzero tint;
- 584 states request visibility other than 100%;
- 506 states request scale other than 100%.

Raw draw-layer distribution is:

`defa=156, grou=17, grhi=68, ailo=10, aihi=51, plwe=5, play=0, plsh=2,
plef=0, plui=10, atmo=0, hud=17, none=50, other=0`.

## Downstream boundary

`RENDER_BACKEND_RUNTIME.md` now closes the software clipping/blitting, scaled sampling, request queue, `0x18A40/0x19570` submission, and per-request terrain-target compositor below this semantic intent layer. The former live `+0x35` mystery is the request's direct/immediate selector, not a separate rendering backend.

`RENDER_ORCHESTRATION_RUNTIME.md` now closes the semantic-to-raw request bridge for the recovered entity/player sprite paths, including frame resolution, world/HUD/terrain coordinate selection, effect-color packing, immediate/queued routing, and the exact `0x100B0` horizontal-view stepper. Terrain/background surface lifetime and scrolling are now reconstructed in `TERRAIN_SURFACE_RUNTIME.md`: the persistent full raster is copied as a complete 416x480 viewport, not incremental strips. The proven top-level world-composition sequencing is now bound in `PARTICLE_WORLD_RENDER_RUNTIME.md`; remaining renderer work is particle producer/update semantics and replacement of legacy display/presentation ownership without changing the proven compositor arithmetic.
