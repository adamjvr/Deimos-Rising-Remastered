# Legacy software render backend runtime

This document records the clean-room reconstruction of the Mac 1.0.6 software
sprite compositor below `0x18A40` / `0x19570`. The clean implementation uses
explicit portable xRGB1555 surfaces rather than reproducing QuickDraw object
ownership, but preserves the observable request, queue, clipping, sampling, and
pixel-arithmetic contracts recovered from the PPC executable.

## 76-byte request record

The original renderer passes a fixed `0x4C`-byte record. The recovered fields
used by the software path are:

| Offset | Meaning |
|---|---|
| `+0x00` | cached/resolved frame pointer |
| `+0x04/+0x08` | center X / Y |
| `+0x0C` | sprite FourCC |
| `+0x10` | frame index |
| `+0x14` | effect/target flags |
| `+0x18` | scale float |
| `+0x1C` | effect amount in the legacy `0..32` transparency domain |
| `+0x20..+0x2C` | QuickDraw clip Rect: top/left/bottom/right |
| `+0x30` | numeric render layer |
| `+0x31` | immediate/direct selector |
| `+0x34` | 16-bit effect color |
| `+0x38..+0x44` | rectangle used by the special `COST` path |
| `+0x48` | 16-bit `COST` color |

The relocated request template starts with sprite `none`, scale `1.0`, layer
`7`, effect color `0x7fff`, and otherwise zeroed fields.

Low flag bits consumed by `0x19570` are:

- `0x1` — overall sprite transparency;
- `0x2` — shadow/darken mode;
- `0x4` — solid-color tint/glow mode;
- `0x8` — terrain/background destination.

When several effect bits are present, the original dispatch priority is
`0x1`, then `0x2`, then `0x4`. Normal 1.0.6 request construction emits them as
separate passes.

## Submission and queueing

`0x18A40` is the outer submission wrapper. With Sprite FX enabled, a request
with `+0x31 != 0` is rasterized immediately through `0x19570`; otherwise the
complete 76-byte record is copied into the layer queue through `0x1A450`.
Queued copies have `+0x31` forcibly set to one.

`0x1A650` flushes one numeric layer. Layers 0 and 1 are one-shot terrain
layers: after rasterization their queued sprite FourCC is replaced by `none`.
Higher layers remain resident until the broader queue is reset.

`0x18B20` flush groups are exact:

- group 0 -> layers 0..1;
- group 1 -> layers 2..5;
- group 2 -> layers 6..15;
- values >=3 do nothing.

The clean `LegacyRenderQueue` models these semantics without reproducing the
legacy allocation/list implementation.

## Original renderer toggles

Two formerly anonymous globals are named by executable diagnostic strings:

- `Sprite FX Enabled` / `Sprite FX Disabled` (`0x1AFC0`), default enabled;
- `Sprite Alpha Drawing Enabled` / `Sprite Alpha Drawing Disabled`
  (`0x1F040`), default enabled.

When Sprite FX is disabled, `0x18A40` copies the request and forces:

- scale = `1.0`;
- effect amount = `0`;

while preserving the remaining flags and the immediate/queued routing.

When Sprite Alpha Drawing is disabled, the secondary 0..32 transparency plane
is ignored and the compositor falls back to the frame's 16-bit transparent
color key.

## Geometry and clipping

At scale exactly `1.0`, top-left is computed from the stored integer frame
size and signed truncating half extents.

For scale other than one, `0x1A6F0` / `0x1AA90` use:

- `scaledW = trunc(srcW * scale)`;
- `scaledH = trunc(srcH * scale)`;
- left/top are centered using the **untruncated floating scaled size** before
  PPC `fctiwz`;
- right/bottom are left/top plus the truncated scaled extent.

Scaled sampling is nearest-neighbor integer-ratio mapping:

```text
sx = srcW * (destX - left) / scaledW
sy = srcH * (destY - top)  / scaledH
```

The original has distinct fully-inside and clipped routines. The clean backend
uses one bounded loop but retains QuickDraw's right/bottom-exclusive Rect
semantics and the same source mapping over the complete, unclipped scaled rect.

## xRGB1555 blending

For a destination-weight transparency `t` in `0..32`, each 5-bit channel uses:

```text
out = floor((dst * t + src * (32 - t)) / 32)
```

The reconstructed secondary transparency plane uses:

- `0` = fully opaque source;
- `1..31` = blend;
- `32` = fully transparent;
- `1000` in the first word of a row = skip the entire row.

If no plane is used, source pixels equal to the frame transparent key are
skipped.

## Four compositor families

### Normal source

Plane value zero copies source. Values 1..31 blend source and destination by
the plane transparency. Value 32 and sentinel rows do not write.

### Overall transparency

The per-request amount and per-pixel transparency are **added**, not
multiplied:

```text
effective = requestTransparency + pixelTransparency
```

Values >=32 do not write; otherwise the normal xRGB1555 blend uses the
effective value.

### Shadow

Shadow mode never draws source RGB. Source coverage only selects which
existing destination pixels are darkened.

For an opaque source pixel, retained brightness is the request's base amount.
For a partial mask value `m > 0`, PPC `0x1DDF0` uses the literal single-precision
constant `0.032f`:

```text
factor = trunc(base + m * (0.032f * m))
```

A factor >=32 leaves the destination unchanged. Otherwise every destination
5-bit channel is multiplied by `factor/32` with integer truncation.

### Solid-color tint/glow

Source RGB is ignored except for coverage. The request's 16-bit effect color
is blended toward the destination with:

```text
effective = requestTransparency + pixelTransparency
```

again skipping at >=32.

## Visual percentage conversion

`0x10C20` converts the live percentage domain to the raw request domain. The
PPC caller first truncates the live visibility to an integer percentage:

```text
transparency = trunc(abs((trunc(percent) / 100.0) * 32.0 - 32.0))
transparency = min(transparency, 32)
```

Examples: 100% -> 0, 80% -> 6, 0% -> 32, 150% -> 16.

Tint request construction uses the already-computed base visibility
transparency to reduce tint coverage before converting back to a destination
weight:

```text
rawTintCoverage  = 32 * (tintPercent / 100)
visibleFraction  = 1 - baseVisibilityTransparency / 32
effectiveCoverage = rawTintCoverage * visibleFraction
tintTransparency = trunc(abs(effectiveCoverage - 32))
```

Collision glow live `+0x78` is already a raw `0..32` request-domain amount and
is not a percentage.

## Terrain target

`stateDrawToTerrain` main/tint/glow requests use target flag `0x8` and one-shot
layer 1. Terrain shadow requests use the same target flag and one-shot layer 0.
The compositor arithmetic itself is identical; the request chooses a separate
terrain/background xRGB1555 destination surface.

Sprite-base `+0x90` remains the strict global-sequence submission gate upstream
of this backend. The clean implementation now covers the per-request terrain
pixel composition; ownership, scrolling/persistence, dirty-region presentation,
and final display of the complete background surface remain separate world/
platform orchestration work.

## Special `COST` path

Sprite FourCC `COST` bypasses frame sampling and enters `0x1EC80`, which clips a
request rectangle and blends a constant 16-bit color using the request
transparency. No stock canonical `COST` sprite occurrence has been found, but
the executable-supported path is preserved synthetically.

## Canonical compositor oracle

The reference probe runs six deterministic render variants against each of the
2,460 reconstructed stock frame surfaces:

1. normal alpha-plane source;
2. overall transparency;
3. shadow;
4. solid-color tint/glow;
5. 1.5x nearest-neighbor scaling;
6. alpha drawing disabled / transparent-key fallback.

That produces **14,760 canonical software-render passes** with aggregate FNV64:

```text
0x32290b39b091e970
```

The source-frame surface oracle remains `0x9f9dcfba05b5089c`.

## Remaining renderer boundary

The sprite software compositor and ordinary terrain-target composition are now
reconstructed. Remaining work is primarily orchestration/platform ownership:

- convert every semantic world/player render intent into the typed raw request
  with exact clip/destination ownership at the original call sites;
- reconstruct complete terrain/background surface lifetime, scroll/persistence,
  and presentation rather than just per-request rasterization;
- recover any remaining non-sprite/UI presentation special cases;
- replace legacy QuickDraw/Sound Manager display ownership with a native
  presentation layer without changing the proven software-render arithmetic.
