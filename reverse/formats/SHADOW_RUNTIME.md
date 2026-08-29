# Shadow Transform Runtime — Mac 1.0.6

Status: **exact shadow request geometry, scale, layer, and legacy transparency mapping reconstructed; downstream raster/backend submission remains open**.

This document closes the transform portion of PPC `0x13460`. The clean runtime can now generate the same deterministic shadow request geometry that the original Mac engine passed to its renderer, including air/ground offsets, scale behavior, horizontal view shift, terrain submission coordinates, draw layer, and the legacy 0–32 transparency value.

## Recovered function/data map

| PPC routine/data | Recovered role |
| --- | --- |
| `0x13460` | construct one shadow request |
| `0x10C20` | map visibility percentage into the legacy 0–32 transparency domain |
| `0x20250` | access fixed `Game[gafl]` values used by the shadow transform |
| `0x100A0` | return bounded horizontal view offset used by world-space render transforms |
| `0xFEC0` | current world/background Y origin |
| PEF literal `0.5f` | fixed air-shadow scale basis |

## Fixed `Game[gafl]` shadow configuration

The four positional entries read by `0x13460` are label-verified in canonical data:

| Index | Label | Value |
| ---: | --- | ---: |
| 48 | `Shadow_XOffset` | `-48` |
| 49 | `Shadow_YOffset` | `104` |
| 50 | `Shadow_GroundXOffset` | `-6` |
| 51 | `Shadow_GroundYOffset` | `8` |

The clean `compile_legacy_shadow_runtime_config()` verifies those labels before accepting the table.

Nearby canonical entries are `MinScreenWidth=640`, `MinScreenHeight=480`, `VisibleGameWidth=416`, and `VisibleGameHeight=480`, further anchoring this table region as view/render configuration.

## Air shadows

Air-domain shadows use:

```text
shadowScale = 0.5 * entityScale
```

When `adjustShadowLocForScaling_BOOL` is **false**, the offset basis remains the fixed 0.5 air-shadow scale regardless of entity scale:

```text
xOffset = trunc(-48 * 0.5) = -24
yOffset = trunc(104 * 0.5) = 52
```

When the flag is **true**, the offsets scale with the actual shadow scale:

```text
xOffset = trunc(trunc(Shadow_XOffset) * shadowScale)
yOffset = trunc(trunc(Shadow_YOffset) * shadowScale)
```

Canonical Mac 1.0.6 data contains zero Unit Definitions enabling this flag, so the branch is preserved by synthetic regression rather than stock-corpus execution.

## Ground shadows

Ground-domain shadows use the entity scale directly:

```text
shadowScale = entityScale
xOffset = trunc(trunc(Shadow_GroundXOffset) * entityScale)
yOffset = trunc(trunc(Shadow_GroundYOffset) * entityScale)
```

`adjustShadowLocForScaling_BOOL` does not alter this ground formula.

## Offset domain and shadow layers

`defa` chooses the air/ground domain from the entity's cached air-domain bit. Explicit layer IDs can override that domain:

```text
default ground / grou -> shadow layer 2
grhi                  -> shadow layer 4
default air            -> shadow layer 6
recognized air/player/HUD layers -> shadow layer 6
```

Explicit `grou`/`grhi` therefore force ground offset behavior; recognized non-ground layers force the air offset domain. Unknown layer IDs retain the air/ground offset domain prepared from the entity bit while leaving the numeric layer at its legacy default.

Terrain submission is a separate layer-0 path.

## Position transform

For an ordinary world-space shadow request:

```text
x = trunc(worldX + xOffset - horizontalViewOffset)
y = trunc(worldY + yOffset)
```

The horizontal view offset is applied only when the live world-space transform gate (`+0x18`) is enabled. HUD/non-world-space objects therefore do not receive that shift.

`0x100A0` returns an integer bounded to `[-32,31]`. PPC `0x100B0` is now reconstructed as its exact step controller: every accepted step changes the offset by one pixel, writes a direction latch of `-1` or `+1`, and clamps at `-32/31`; a request that would cross a hard limit leaves the direction latch at zero. The clean runtime therefore models the proven renderer-facing horizontal-view state without assigning a broader gameplay-camera meaning that has not been demonstrated.

## Terrain submission transform

When the current state requests terrain submission:

```text
x = trunc(worldX + xOffset - 32)
y = trunc(worldY + yOffset + worldYOrigin)
layer = 0
```

This bypasses the ordinary horizontal-view-offset path and uses the same recovered `0xFEC0` world/background Y origin already present in the terrain runtime.

## Visibility to legacy transparency — `0x10C20`

The caller first truncates visibility to an integer with PPC `fctiwz`. The helper then computes:

```text
mapped = abs((visibilityInt / 100.0) * 32.0 - 32.0)
transparency = trunc(mapped)
transparency = min(transparency, 32)
```

`0x13460` subsequently enforces:

```text
transparency = max(transparency, 20)
```

So the shadow request uses the old 0–32 transparency domain, where larger values are more transparent, and the original engine never lets this path request a shadow more opaque than the value corresponding to `20`.

## Clean API

`LegacyShadowRuntimeConfig` stores the four label-verified Game-table offsets. `build_legacy_shadow_request_geometry()` consumes the visual runtime, world position, horizontal view offset, and world Y origin and returns the exact deterministic request facts:

- transformed x/y;
- shadow scale;
- numeric shadow layer;
- 0–32 transparency;
- terrain-submission flag.

No QuickDraw or platform renderer is required for these calculations.

## Canonical validation

The canonical probe verifies the four fixed shadow values exactly:

```text
shadow offsets air/ground: -48,104 / -6,8
```

Canonical `Game.pak` contains 67 Unit Definitions with `castsShadows_BOOL`, providing stock coverage of shadow eligibility/layer use. The unused `adjustShadowLocForScaling_BOOL` branch is separately regression-bound.

## Still open

The following are deliberately outside this boundary:

- exact destination clipping/software-blitter internals;
- backend dispatch/submission below `0x18A40` / `0x19570`;
- the remaining alternate renderer submission selector;
- bind the recovered full-terrain viewport/frame-loop choreography and native presentation after terrain-target composition.

Shadow position, scale, layer selection, and transparency are no longer open renderer questions.
