# Terrain Surface Runtime — Mac 1.0.6

This milestone closes the core background-surface/camera boundary around PPC
`0xFA10`, `0xFA90`, `0xFBC0`, `0x10000`, `0x10120`, and `0x10220`.

The clean implementation is in:

- `include/deimos/terrain_runtime.hpp`
- `src/core/terrain_runtime.cpp`
- `tests/terrain_surface_runtime_test.cpp`

The most important correction is architectural: **the original gameplay path
does not scroll the background by copying incremental bitmap strips.** The game
owns a persistent full terrain/background surface, changes a source-view Rect as
the camera advances, and copies the **entire 416x480 visible crop** into the
main gameplay surface whenever `0x10120` runs. Persistent terrain sprite writes
therefore survive because they mutate the full terrain surface itself.

## `0xFBC0` — persistent background surface

`0xFBC0` loads the level's `im16` background, queries the decoded image width
and height, verifies those dimensions against the level/background context, and
passes `Game[gafl]` index 56 to the surface resize/allocation helper `0x9D70`.
For the canonical 1.0.6 data that value is:

```text
Game[gafl][56] ReqDisplayDepth = 16
```

The loaded image is then copied into the persistent destination through
`0x9FD0`, after which the temporary image is disposed. The destination pointer
used later by `0x10120` is therefore the long-lived terrain/background raster,
not a transient strip buffer.

The clean compositor already represents these surfaces as 16-bit
`LegacyRasterSurface` objects. `LegacyTerrainSurfaceRuntime` intentionally owns
only camera/scroll state; the caller retains the full persistent raster so the
same object can receive layer-0/layer-1 terrain requests across frames.

## Label-verified gameplay dimensions

`compile_legacy_terrain_surface_config()` verifies the exact positional
`Game[gafl]` contract used by the executable:

| Index | Label | Canonical value |
| ---: | --- | ---: |
| 54 | `VisibleGameWidth` | 416 |
| 55 | `VisibleGameHeight` | 480 |
| 56 | `ReqDisplayDepth` | 16 |

Two other dimensions are executable literals in this path rather than table
lookups:

- horizontal source bias: `+32` pixels;
- ahead-of-camera row activation margin: `64` pixels.

The real-corpus `deimos_reference_probe` now verifies the canonical contract as
`416x480x16`.

## `0xFA90` — initial source view

After `0xFBC0` has populated the persistent background, `0xFA90` initializes the
full terrain bounds and source-view Rect. In QuickDraw ordering the clean
semantic equivalent is:

```text
fullBounds = { top=0, left=0, bottom=terrainHeight, right=terrainWidth }

sourceView.left   = 32
sourceView.top    = fullBounds.bottom - VisibleGameHeight
sourceView.right  = sourceView.left + VisibleGameWidth
sourceView.bottom = sourceView.top  + VisibleGameHeight
```

For a 480-wide background the initial X crop is therefore `[32,448)`. The
horizontal controller `0x100A0/0x100B0` can move that source window left/right
without moving entity world coordinates.

Scroll state initializes as:

```text
requestedVerticalDelta = +1
appliedVerticalDelta   = 0
verticalProgress       = VisibleGameHeight + 1
reachedEnd             = false
horizontalOffset       = 0
horizontalDirection    = 0
```

Canonical `VisibleGameHeight=480`, so `verticalProgress` starts at **481**.
That `+1` is observable later in the end-of-level scroll boundary.

## `0xFA10` — row activation, not bitmap strip copying

`0xFA10` calls world routine `0x33090` over a range extending 64 pixels above
the current source view. Its loop is exactly equivalent to:

```text
for i = 0; i < sourceBottom - (sourceTop - 65); ++i
    updateRow(sourceBottom - i)
```

For a 480-high viewport this produces **545 callbacks**:

```text
sourceBottom, sourceBottom-1, ... sourceTop-64
```

This is the source of the apparent "strip" behavior seen near the scrolling
code, but it is a **world/simulation row-activation boundary**. It does not copy
terrain pixels. `prime_legacy_terrain_rows()` mirrors this exact range and
honors the executable's suppression flag.

## `0x10220` — vertical source-Rect motion

`0x10220` does not draw. It changes only the camera/source Rect and scroll
accounting:

1. if requested delta is zero, set applied delta to zero and return;
2. save the old source top;
3. subtract the requested delta from source top and bottom;
4. add the requested delta to vertical progress;
5. clamp progress to `[0, fullBounds.bottom]`;
6. if source top is `<= 0`, force `{top=0,bottom=VisibleGameHeight}`;
7. query the persistent terrain surface bounds;
8. if source bottom exceeds the terrain bottom, clamp bottom and reconstruct
   top as `bottom-VisibleGameHeight`;
9. store `appliedVerticalDelta = oldTop-finalTop`.

The clean `step_legacy_vertical_terrain_view()` preserves that ordering. The
applied delta is important because `0xFED0` exposes it to other subsystems;
`0x2A7A0` uses it to vertically shift the persistent ground-obstacle Rect list.

## `0x10000` — scroll tick and the one-pixel end quirk

`0x10000` first calls `0x10220`. When scrolling is still requested, it compares
`verticalProgress` with the full terrain bottom. On reaching the limit it:

- clamps progress to the full bottom;
- latches the reached-end byte;
- clears requested vertical delta;
- returns true;
- skips the row-activation callback for that tick.

Otherwise, when row updates are not suppressed, it calls:

```text
0x33090(sourceView.top - 64)
```

Because normal scrolling starts with progress `481`, a `+1` scroll reaches
`progress == terrainHeight` while `sourceView.top == 1`. The ordinary gameplay
end latch therefore occurs **one pixel before top==0**. This is not cleaned up
in the portable runtime; the regression test preserves it deliberately.

## `0x10120` — full 416x480 viewport copy

Direct disassembly of `0x10120` resolves the previous strip-copy/dirty-region
ambiguity. It clones the current source-view Rect, then replaces only its
horizontal coordinates:

```text
source.left = horizontalOffset + 32
if source.left < 0:
    source.left = 0
source.right = source.left + Game[gafl][54]
```

It constructs a destination Rect:

```text
{ top=0, left=0, bottom=Game[gafl][55], right=Game[gafl][54] }
```

and invokes `0x9FD0` from the persistent terrain/background surface to the main
visible gameplay surface. For canonical data this is an unscaled **416x480
copy every call**.

`copy_legacy_terrain_viewport()` reproduces this operation on the portable
16-bit surfaces, including the original horizontal offset range. Tests mutate a
pixel in the persistent background, perform repeated viewport copies, and prove
the mutation persists and reappears at the expected visible coordinate.

## Proven top-level ordering around the terrain copy

Direct disassembly of the world renderer around `0x30BC0` bounds the exact
composition order now implemented by `render_legacy_world_frame()`:

```text
0x2DEA0(...)
0x18B20(group 0)       # layers 0..1: one-shot terrain shadow/main writes
0x10120()              # full persistent-terrain -> visible viewport copy
0x18B20(group 1)       # layers 2..5
0x43BA0(...)            # direct 7x7 particle raster into visible surface
0x18B20(group 2)       # layers 6..15
```

This proves why terrain writes must be flushed before the background viewport
copy: layer 0/1 mutations become part of the persistent terrain raster, then
the newly updated raster is copied into the visible gameplay surface before
ordinary sprite layers are composited.

The complete recovered outer composition segment is now implemented by
`render_legacy_world_frame()`: all three queue groups, the full terrain copy,
and the exact `0x43BA0` particle pass preserve the `0x30BC0` ordering and draw
latch. The downstream legacy QuickDraw presentation-copy geometry is now recovered separately in `NATIVE_PRESENTATION_RUNTIME.md`.

## Validation

`terrain_surface_runtime_test` covers:

- label-verified 416x480x16 configuration;
- exact bottom-most initial source Rect and progress 481;
- all 545 `0xFA10` prime-row callbacks;
- row-update suppression;
- full-viewport copying at horizontal offsets -32, 0, and +31;
- persistence of terrain mutations across repeated copies;
- exact +1 vertical source motion and applied-delta reporting;
- the one-row `top-64` activation callback;
- zero-request behavior;
- the normal source-top==1 end latch;
- top and bottom clamp behavior from `0x10220`;
- invalid/small destination rejection in the clean wrapper.

The repository suite is **38/38 PASS in Debug**, and the canonical `Game.pak`
probe confirms `416x480x16` while preserving the established gameplay oracles:
386 groups, 546 constructed live members, 544 active after the first tick, and
RNG seeds `2249411936` / `2633739833`.

## Remaining terrain/render boundaries

- particle construction/update and state/hit/destruction producers plus immediate QuickDraw presentation geometry are now closed; continue into score-bar/UI production and final destination-buffer/swap ownership;
- recover score-bar/UI producers and any other non-sprite special paths that populate the source canvas outside the closed `0x30BC0` composition segment;
- bind a decoded Media Mask provider instead of the current clean callback;
- map the recovered presentation plan to modern native backends without changing the recovered software-render arithmetic.
