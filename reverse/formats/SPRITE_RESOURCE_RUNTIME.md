# Sprite Resource Cache and Atlas Runtime — Mac 1.0.6

Status: **legacy sprite cache, GIF-index decode, atlas extraction, 16-bit frame surfaces, and geometry lookup reconstructed; backend clipping/blitting remains open**.

This milestone closes the resource side of the previously recovered visual/render-request boundary. The clean core can now decode the original indexed GIF sprite plates, reproduce the Mac atlas-rectangle scanner, construct the original cropped 16-bit color/transparency frame surfaces, publish complete sprite groups into a deterministic cache, resolve frames with the original fallback rules, compute PPC-compatible scaled dimensions, and feed those dimensions back into the `0x12940` live geometry refresh.

The frame-surface layout and alpha semantics are detailed in `SPRITE_FRAME_BITMAP_RUNTIME.md`. QuickDraw destination clipping, software blitting, renderer queues, and backend submission remain downstream boundaries.

## Recovered function map

| PPC routine | Recovered role |
| --- | --- |
| `0x18D20` | load/build one sprite group and publish it only after all frames succeed |
| `0x19530` | loaded-group test through frame-zero lookup |
| `0x19AD0` | find loaded group/frame; high frame index falls back to frame 0 |
| `0x19C10` | return scaled dimensions from an already-resolved frame |
| `0x19CA0` | lazy dimension lookup; load absent group and retry |
| `0x19EE0` | return loaded frame count, or zero if absent |
| `0x1F140` | alpha-plate frame-list wrapper/validation |
| `0x1F1C0` | row-major plate-frame list builder |
| `0x1F340` | locate and trim the next frame cell |
| `0x1F4E0` | find the next horizontal separator-marker row |
| `0x1F540` | find the next full separator-marker column |
| `0x1F5B0` | trim a candidate cell using that cell's own corner value |
| `0x12940` | refresh scaled sprite dimensions and live half-extents |

## Loaded sprite-group contract

The Mac loader publishes a 16-byte group record:

| Offset | Meaning |
| ---: | --- |
| `+0x00` | runtime type/marker; loader writes `0x499602D2` |
| `+0x04` | sprite-group FourCC |
| `+0x08` | frame count |
| `+0x0C` | pointer to the frame-pointer list |

Each resolved frame object begins with width and height at `+0x04/+0x08`. State entry `0x146F0` calls `0x19AD0(face, frame)` and stores the resulting frame pointer in live sprite-base `+0x50`, proving that field is the cached current-frame handle.

The clean `LegacySpriteCache` keeps the ownership model portable but preserves the observable contract:

- a group becomes visible only after a complete frame list has been constructed;
- duplicate publication is rejected rather than exposing partial replacement state;
- `none` never resolves to a frame;
- a requested frame `>= frameCount` resolves to frame 0;
- `frame_count()` returns zero for an absent group;
- lazy dimension lookup asks the loader once for an absent group, then retries the lookup.

### Negative frame safety divergence

The original `0x19AD0` signed comparison lets a negative frame index pass its upper-bound test and would index before the frame-pointer array. That is memory-unsafe legacy behavior, not useful game semantics. The clean cache rejects negative frame indices instead of reproducing an out-of-bounds read. This is an explicit safety divergence.

## Scaled dimensions — `0x19C10` / `0x19CA0`

For scale exactly `1.0`, the engine returns the stored frame width/height directly. Otherwise each axis is multiplied by scale and converted with PPC `fctiwz`, i.e. truncation toward zero:

```text
scaledWidth  = trunc(frameWidth  * scale)
scaledHeight = trunc(frameHeight * scale)
```

`0x19CA0` special-cases the `none` sprite to `0,0`. If the requested group is not loaded it attempts the resource load, then retries the lookup. A high requested frame therefore falls back to frame 0 after that load, just as it does for an already-loaded group.

## Exact indexed-GIF input domain

The original plate scanner does not reason about RGB colors. It consumes the decoded 8-bit **palette indices** from the alpha GIF directly. The clean decoder therefore preserves index bytes rather than converting the image to RGBA.

`decode_legacy_gif_indices()` supports the stock GIF87a/GIF89a features required by the original plates:

- logical-screen dimensions;
- global or local color tables;
- extension skipping;
- GIF LZW image data;
- interlaced row order;
- first-image compositing into an indexed logical-screen canvas.

Color-table RGB values are deliberately irrelevant to the **atlas marker/trimming algorithm**, but the decoder now preserves them as packed xRGB1555 pixels because the recovered `0x1D780` frame builder consumes the color plate after rectangle extraction.

## Exact atlas grammar — `0x1F140..0x1F5B0`

The alpha plate's second decoded byte is the separator marker. The scanner first requires the first three bytes to be pairwise suitable for the legacy marker convention (`byte0 != byte1` and `byte1 != byte2`). It then walks the image in row-major cell order.

For each row band:

1. `0x1F4E0` advances from the current y position to the next row whose first-column byte is the separator marker.
2. `0x1F540` advances across that band until it finds a **full column** consisting entirely of the separator marker.
3. The candidate cell between marker boundaries is passed to `0x1F5B0`.
4. `0x1F5B0` takes the candidate cell's own top-left palette index as its local trim/background value and removes complete matching rows/columns from all four sides.
5. The resulting content rectangle is appended to the frame list and scanning continues to the next cell.

For the recovered scan variables, the emitted source rectangle is:

```text
left   = x + 1
top    = y + (bandHeight - contentHeight) - 1
right  = left + contentWidth
bottom = top + contentHeight
```

This reproduces the deliberately nonuniform rectangles found in some stock plates; the atlas is not simply a fixed grid.

## Canonical examples

Exact clean replay against Mac 1.0.6 assets produces:

- `PL1B` / Player 1 Blue: 7 frames; frame 0 is 53×43;
- `EXLG` / Explosion - Large: 12 frames with varying trimmed bounds;
- `BOCR` / Bomb Crater: 3 frames;
- `GLOW` / Glows: 12 frames.

For example, `BOCR` yields source rectangles:

```text
(3,3)-(19,19)   16x16
(22,3)-(37,19)  15x16
(40,7)-(53,19)  13x12
```

The variable sizes are produced by the original cell-local trimming rule, not by post-processing in the clean implementation.

## Corpus-wide validation

The canonical `Game.pak` resource corpus currently validates:

```text
alpha plates:                       124
color plates:                       124
matched alpha/color pairs:          123
extracted alpha rectangles:        2463
paired 16-bit frame surfaces:      2460
frames with transparency plane:    2460
color/transparency words: 3115564 / 3115564
row-skip sentinels:                 6341
sprite-surface FNV64: 0x9f9dcfba05b5089c
```

All 123 alpha/color pairs that both exist have equal plate dimensions. Stock alpha tags are uppercase while their paired color tags are lowercase, so the probe verifies identity by ASCII case-folding. The stock exception is `PDLI` (`Plasma Drone Light`), which has three alpha rectangles but no matching color plate; those three account for the difference between 2,463 alpha rectangles and 2,460 normal color frame surfaces.

The corpus-wide extraction and surface hash exercise the recovered marker/trimming grammar, palette conversion, transparent-key selection, transparency weights, and row layout over thousands of frames rather than a hand-selected handful of sprites.

## `0x12940` geometry integration

The clean render runtime now consumes the sprite cache directly when live geometry is dirty:

1. if the face is `none`, set half-width/half-height to zero, clear the dirty flag, and leave the prior width/height untouched;
2. otherwise resolve the current face/frame through the loaded cache or lazy loader;
3. scale dimensions with the exact `0x19C10` truncation rule;
4. write live width/height;
5. divide each signed dimension by two with truncation toward zero, matching the PPC sign-adjust/shift sequence;
6. clear the dirty flag.

The stale width/height behavior for a `none` face looks odd but is present in the original `0x12940` path and is regression-bound.

## Still open

The sprite-resource and source-frame boundaries no longer include bitmap construction or shadow geometry. Remaining downstream work is:

- exact clipping and destination-surface arithmetic in the software blitters;
- backend identities/dispatch beneath `0x18A40`, `0x19570`, and the alternate submission path;
- actual terrain/background pixel composition behind the live `+0x90` terrain-submission sequence;
- original QuickDraw allocation/lifetime details only where they produce a gameplay- or renderer-visible contract.

See `SPRITE_FRAME_BITMAP_RUNTIME.md` for the recovered xRGB1555 frame object and transparency plane, and `SHADOW_RUNTIME.md` for the exact `0x13460` shadow transform.
