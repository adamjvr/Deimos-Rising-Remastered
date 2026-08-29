# Score-bar pixel runtime — Deimos Rising 1.0.6

## Scope

This note closes the pixel-generation half of the 160x480 gameplay score bar.
The previous score-bar milestone reconstructed semantic caches, dirty regions,
resource identities, score/life production, and meter convergence. This pass
binds those semantics to the original interface pixels and the recovered
small-text renderer.

Primary PPC routines:

- `0x31AE0` dirty-region dispatcher;
- `0x31D70` score text (`"%0.7i"`);
- `0x31EA0` life symbol;
- `0x32050` life-count text (`"%i"`);
- `0x32250` shield meter;
- `0x32500` power meter;
- `0x327B0` weapon previews;
- `0xE270..0xE8D0` Text Format / glyph request path;
- `0x19570` shared sprite compositor used by text and score-bar sprites.

## Interface.pak is part of the canonical interface tier

The small HUD font is not in `Game.pak`. The original installation contains a
separate `Interface.pak` (2,849,896 bytes in the recovered Mac 1.0.6 corpus).
It supplies:

- `im08/Text - Small IA[TESM].gif`;
- `im08/Text - Small IC[tesm].gif`.

The runtime Text Format path resolves `tesp` to the loaded `tesm` sprite group.
The two 852x18 plates reconstruct exactly **91 frames**. This closes the prior
font-source ambiguity without substituting a host font.

The static score-bar background remains canonical `Game.pak` resource:

- `im16/Scorebar[scor].TGA` — exactly **160x480**, 16-bit.

## 16-bit TGA path

`image16_resource.hpp/.cpp` implements the uncompressed type-2 16-bit TGA family
used by the interface surface. The decoder:

- validates dimensions and payload bounds;
- normalizes bottom-left/top-left TGA origins to top-left clean surfaces;
- rejects right-to-left images for the proven 1.0.6 contract;
- retains the low 15 xRGB1555 bits consumed by the recovered compositor.

This keeps interface images in the same xRGB1555 domain as reconstructed sprite
and terrain pixels.

## Text-format semantics

The recovered Text Format runtime bytes relevant here are now named:

- `Monospaced`;
- `DrawShadows`;
- blend amount;
- scale;
- spacing;
- Colorise and Colorise RGB.

Canonical score/life formats are monospaced, do not draw shadows, and route each
glyph through `0x19570` with solid-color colorisation.

### Character dispatch

`0xE8D0` maps printable characters into the `tesm` frame group. The clean
`legacy_small_text_frame_for_char()` preserves that dispatch. Important score
bar identities include:

- `1..9` -> frames 52..60;
- `0` -> frame 61;
- space -> layout-only, no frame.

The canonical digit widths (`1..9,0`) are:

```text
5, 6, 7, 7, 7, 7, 6, 7, 7, 7
```

and each digit frame is 13 pixels high.

## Score and lives formatting

The original C formatting strings are exact:

```text
score: "%0.7i"
lives: "%i"
```

Therefore score `12345` is submitted as `0012345`.

Normal player score/life text color is canonical cyan:

```text
RGB 0x94,0xDE,0xE6
xRGB1555 0x4B7C
```

When `clamp(lives-1,0,9)` reaches zero while content is visible, the life-count
format switches to the dedicated last-life style:

```text
RGB 0xFF,0x00,0x00
xRGB1555 0x7C00
```

This is a semantic style switch, not a generic fade.

## Hidden-content fade

The score-bar hide transition does not replace all element behavior with one
alpha rule. For text, the recovered path moves the blend amount halfway toward
full transparency 32 using signed division-by-two semantics. A normal zero-blend
format therefore becomes blend 16 on the hidden redraw.

Life symbols use overall transparency 16 on the hidden redraw. Weapon previews
are restored from the static panel but not re-submitted while hidden.

## Dirty-region pixel restoration

Before drawing each dirty class, the original restores its corresponding
`Rects[inre]` region from the static `scor` panel. The clean rasterizer copies
that panel-local rectangle into source-canvas X `416..575`, then draws the
updated element.

This is essential for deterministic redraws: old glyphs, meter fill, or weapon
sprites are removed by restoring the original background rather than by host UI
clearing.

## Weapon previews

`0x327B0` routes all three weapon previews through the shared compositor:

- slot 0: scale 1.0, transparency/blend 6;
- slots 1/2: scale 0.7, transparency/blend 16.

The preview face/frame descriptors remain those compiled from each Weapon
Definition.

## Shield and power meter pixels

The score-bar first restores the dirty meter region and re-draws the canonical
player shield/power sprite. When the displayed percentage is below 100, the
recovered `COST` solid-rectangle path masks the unfilled part using the Text
Format strip style. Canonical meter strip style is black with blend amount 8.

The semantic percentages still come from the already-recovered cache updater:

- shield converges +2 / -3;
- power converges +2 / -4 and clamps 0..100.

The pixel stage consumes the cached percentage; it does not own meter timing.

## Original-asset-backed oracle

When canonical `Interface.pak` is present beside `Game.pak`,
`deimos_reference_probe` additionally loads the `TESM/tesm` plates, extracts
91 frames, verifies canonical digit metrics, decodes the 160x480 `scor` TGA,
and renders a fixed score/last-life sample through the clean compositor.

The resulting score-bar-region FNV64 is:

```text
0xd2f48984985f54d8
```

This is independent of the existing sprite-surface and complete software-render
oracles and therefore adds a new interface-pixel regression boundary.

## Clean implementation

Files:

- `include/deimos/image16_resource.hpp`;
- `src/core/image16_resource.cpp`;
- pixel APIs in `score_bar_runtime.hpp/.cpp`;
- `tests/score_bar_pixel_runtime_test.cpp`;
- canonical interface validation in `tools/reference_probe.cpp`.

The synthetic test covers TGA orientation/error handling, character dispatch,
RGB24->xRGB1555 conversion, monospaced glyph placement, exact formatting,
hidden-text fade, last-life red style, score-bar region isolation, meter COST
masking, and weapon/life sprite composition.
