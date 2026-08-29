# Render Orchestration Runtime — Mac 1.0.6

This milestone closes the deterministic bridge between the already-recovered
sprite visual state (`0x12F20 / 0x12FA0 / 0x13460`) and the recovered 76-byte
software-render request consumed by `0x18A40 / 0x19570`.

The implementation is in:

- `include/deimos/render_orchestration.hpp`
- `src/core/render_orchestration.cpp`
- `tests/render_orchestration_test.cpp`

It intentionally stops before native window/GPU presentation. The persistent terrain/background surface and camera lifecycle is closed separately in `TERRAIN_SURFACE_RUNTIME.md`, and `PARTICLE_WORLD_RENDER_RUNTIME.md` now closes the recovered outer world-composition order plus the `0x43BA0` particle raster.

## Raw request construction

For an ordinary main sprite, `0x12FA0` copies the live sprite face/frame,
scale, clip rectangle and immediate selector, then derives center coordinates
from the owning sprite base:

```text
screenX = trunc(worldX) - horizontalViewOffset   (world-space)
screenX = trunc(worldX)                          (HUD/non-world-space)
screenY = trunc(worldY)
```

`horizontalViewOffset` is the integer returned by `0x100A0`.

The main render layer is the already-recovered FourCC mapping (`defa`, `grou`,
`grhi`, `ailo`, `aihi`, `plwe`, `play`, `plsh`, `plef`, `plui`, `atmo`, `hud `).

Base sprites set request flag `0x1` only when the live visibility float is not
exactly `100.0`. The effect amount itself is the recovered 0..32 result from
`0x10C20`, so a non-integral value can technically set the flag while fctiwz
still maps the amount to zero.

Tint and collision glow use flag `0x4` and pack their RGB24 color to xRGB1555:

```text
((red >> 3) << 10) | ((green >> 3) << 5) | (blue >> 3)
```

Request order is exact:

```text
shadow
base sprite (unless stateDoColorise)
tint (when > 0)
collision glow (when active)
```

## Frame resolution

The raw request carries the actual resolved frame object. Clean orchestration
uses the recovered `0x19AD0 / 0x19CA0` cache behavior:

1. use an already-loaded frame when present;
2. lazy-load/retry the group when absent and a loader is available;
3. preserve high-frame-to-frame-zero normalization;
4. emit nothing if the resource still cannot resolve.

This closes the semantic-state -> frame-cache -> raw-compositor chain.

## Main terrain stamp coordinates

A correction to earlier notes is important: **main terrain stamps and terrain
shadows use different X coordinate bases**.

For a `stateDrawToTerrain` main sprite, `0x12FA0` executes:

```text
terrainMainX = trunc(worldX) + 32
terrainMainY = trunc(worldY) + worldYOrigin
layer        = 1
flags       |= 0x8
```

By contrast, the independently recovered `0x13460` terrain-shadow transform
uses its shadow offsets and the fixed `-32` X basis before selecting terrain
shadow layer 0. The clean code keeps those two paths separate.

Sprite-base `+0x90` is the sequence stamp controlling persistent main-terrain
submission. The layer-1/flag-0x8 main write is armed only when:

```text
currentRenderSequence > lastTerrainSubmitSequence
```

and the live stamp is then updated. Shadow construction is independent of this
main-stamp gate, matching `0x13460`.

## Immediate versus queued submission

Sprite-base `+0x35` is copied directly to raw request `+0x31`.

Clean orchestration therefore accepts `immediate` as an outer call-site fact:

- `false`: request enters the recovered 16-layer queue;
- `true`: request goes directly to `0x19570`-equivalent rasterization.

The orchestration helper deliberately does not flush render-layer groups; that
remains owned by the outer world-frame choreography (`0x30BC0` and the three
`0x18B20` phases).

## Horizontal view controller (`0x100A0 / 0x100B0`)

The global behind `0x100A0` is now paired with its exact one-pixel step helper.
`0x100B0`:

1. clears a separate direction latch;
2. adds `+1` or `-1` to the view offset;
3. clamps the offset to `[-32, 31]`;
4. records direction `+1` / `-1` only when the requested movement remains
   inside the bounds;
5. leaves direction `0` when a step saturates at either edge.

This controller is implemented as `LegacyHorizontalViewRuntime` in
`terrain_runtime.hpp` because it directly participates in world/terrain view
composition.

## Tests

`render_orchestration_test` verifies:

- `0x100B0` +/-1 stepping and both saturation edges;
- shadow -> base -> tint -> glow raw-request ordering;
- exact world/HUD horizontal-view behavior;
- xRGB1555 effect-color packing;
- exact base/tint/glow request flags and 0..32 amounts;
- terrain shadow layer 0 vs terrain main layer 1;
- main terrain `+32` X / world-Y-origin coordinates;
- sprite-base `+0x90` one-per-sequence main terrain stamp;
- end-to-end semantic request -> queue -> compositor mutation.

The complete repository suite is **38/38 PASS** and the external canonical
`Game.pak` / `Audio.pak` / `Music.pak` probe remains unchanged, including the
software-render corpus hash `0x32290b39b091e970` and the historical gameplay
RNG/count oracle.

## Remaining renderer work

The remaining renderer frontier is no longer request construction, pixel math,
or terrain surface lifetime. Direct PPC disassembly now proves `0x10120` is a
full 416x480 persistent-terrain viewport copy, while `0x10220` only moves the
source Rect. It is primarily:

- preserve the now-implemented `0x30BC0` order: group 0 terrain writes ->
  `0x10120` full viewport copy -> group 1 -> particle raster -> group 2;
- particle producer/update semantics and immediate legacy presentation geometry are now closed; finish remaining entity/player/world call-site choreography and identify score-bar/UI producers plus final buffer ownership;
- identify remaining UI/non-sprite special presentation paths;
- map the verified 16-bit presentation plan to native platform backends after final buffer/swap behavior is frozen.
