# Sprite Frame Bitmap Runtime — Mac 1.0.6

Status: **16-bit frame-surface construction and legacy transparency-plane semantics reconstructed; backend clipping/blitting remains open**.

This document closes the pixel-storage portion of the sprite loader below the previously recovered GIF atlas and sprite-cache layers. The clean runtime now reproduces the observable frame object produced by the Mac loader: cropped xRGB1555 color pixels, the transparent color key, and the optional 16-bit transparency plane used by the legacy blitters.

It does **not** claim that the QuickDraw destination surfaces, clipping loops, software blitters, renderer queues, or native backend submission have been cloned yet.

## Recovered function map

| PPC routine | Recovered role |
| --- | --- |
| `0x18D20` | sprite-group loader; crops paired alpha/color plates and constructs each frame |
| `0x1EEC0` | build the optional per-pixel transparency plane from the cropped alpha frame |
| `0x1D780` | allocate/populate one 16-bit frame object and adopt its transparency plane |
| `0x1D9F0` / `0x1DB50` family | downstream blitters that prove transparent-key, mask-weight, and row-sentinel semantics |

## Legacy frame object

The frame object created by `0x1D780` has the following recovered observable layout:

| Offset | Meaning |
| ---: | --- |
| `+0x00` | runtime marker `0x499602D2` |
| `+0x04` | width |
| `+0x08` | height |
| `+0x0C` | depth, fixed at 16 |
| `+0x10` | 16-bit transparent color key |
| `+0x12` | secondary transparency-plane-present byte |
| `+0x13` | unresolved byte; no semantic claim is made |
| `+0x14` | byte offset to the optional second plane, or zero when absent |
| `+0x18` | beginning of the main packed 16-bit color plane |

Allocation is therefore functionally:

```text
24 + width*height*2
```

and, when the secondary transparency plane exists:

```text
24 + width*height*2 + width*height*2
```

The clean representation keeps ownership portable while preserving these visible facts.

## xRGB1555 color plane

The paired color GIF is decoded through its palette and packed into QuickDraw-style xRGB1555 words:

```text
pixel = ((r >> 3) << 10)
      | ((g >> 3) << 5)
      |  (b >> 3)
```

Bit 15 is unused by this recovered path.

The loader crops the color plate with the exact rectangle produced by the recovered alpha-atlas scanner and copies the resulting pixels row-by-row into the frame's main plane.

The transparent color key supplied to every frame in a plate is the **third 16-bit pixel of the color plate** (`pixel[2]`) read by the original loader before frame construction.

## Secondary transparency plane — `0x1EEC0`

The alpha plate is also decoded to xRGB1555. For each cropped alpha pixel the legacy routine uses the five-bit red component as an **inverted transparency weight**.

The clean runtime reproduces the following domain:

```text
0       fully opaque
1..31   increasing transparency / blend weight
32      fully transparent
1000    row-skip sentinel, stored in the first word of a fully transparent row
```

The per-pixel rule is:

1. if the alpha pixel equals the supplied transparent-key word, write `32`;
2. otherwise extract `red5 = (pixel >> 10) & 31`;
3. when `red5 < 31`, write `red5` and mark the plane as required;
4. when `red5 >= 31`, write `32`.

After a row is built, if every pixel in that row is fully transparent, its first transparency word is replaced with `1000`. The downstream blitter family checks that value before processing the row.

If **no pixel in the entire cropped frame** ever requires a value below the fully-transparent level, the secondary plane is discarded. The downstream renderer then falls back to transparent-color-key comparison against the main color plane.

This means the plane is not merely an "8-bit alpha" substitute, nor is it reserved only for partially translucent sprites. Fully opaque pixels (`0`) also require it.

## Clean resource representation

`LegacyIndexedImage` now preserves both domains required by the original loader:

- palette-index bytes for the exact atlas marker/trimming algorithm;
- decoded xRGB1555 pixels for frame-surface construction.

`LegacySpriteFrameMetadata` now carries:

- source rectangle and dimensions;
- transparent key;
- packed color pixels;
- optional transparency words.

`build_legacy_sprite_group()` consumes a decoded alpha/color plate pair and builds the complete frame-surface group atomically. The existing cache therefore still publishes a group only after every frame succeeds.

## Canonical corpus oracle

Mac 1.0.6 `Game.pak` contains 123 alpha/color plate pairs that both exist. Replaying the complete crop + color + transparency construction produces:

```text
paired 16-bit frame surfaces:       2460
frames with transparency plane:     2460
color words:                     3115564
transparency words:              3115564
row-skip sentinels (1000):          6341
aggregate surface FNV64: 0x9f9dcfba05b5089c
```

The three-frame difference from the previously reported 2,463 alpha rectangles is the stock `PDLI` alpha-only plate, which has no paired color plate and therefore cannot produce normal color frame surfaces.

The canonical plate naming convention also uses uppercase alpha tags and lowercase color tags; the probe validates all 123 identities by ASCII case-folding rather than incorrectly demanding byte-identical FourCC case.

The aggregate FNV64 covers deterministic frame-surface facts so that later changes to GIF palette conversion, cropping, transparent-key handling, mask interpretation, or row layout cannot silently alter the canonical result.

## Still open

This milestone deliberately leaves downstream renderer work bounded:

- exact clipping and destination-surface arithmetic in the software blitters;
- backend identities and submission semantics below `0x18A40` / `0x19570`;
- alternate submission behavior associated with the remaining renderer selector byte;
- native-presentation ownership after the now-bound persistent-terrain/particle world-composition segment;
- ownership/lifetime details that matter only to the original QuickDraw implementation and have no clean-runtime observable yet.

The source frame pixels and transparency semantics are no longer part of that open boundary.
