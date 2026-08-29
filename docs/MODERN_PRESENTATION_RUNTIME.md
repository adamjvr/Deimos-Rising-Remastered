# Modern Presentation Runtime

## Purpose

The original Mac 1.0.6 display path is now recovered through its final
QuickDraw `CopyBits` commit. The remaster must preserve those recovered pixels
without inheriting DrawSprocket, QuickDraw, Carbon, or any other obsolete host
API.

This runtime is the first clean host-facing seam between those two worlds.

The architectural rule is strict:

```text
simulation / recovered software raster
                |
                v
canonical 640x480 xRGB1555 display frame
                |
                |  immutable reference boundary
                v
modern_presentation_runtime
  xRGB1555 -> RGBA8888
  host viewport plan
                |
                v
ModernPresentationBackend
                |
        +-------+--------+---------+
        |       |        |         |
      Metal   Vulkan    D3D     other host
```

No modern backend is permitted to feed results back into simulation, sprite
composition, terrain, score-bar state, particle state, or the legacy raster.
The xRGB1555 frame remains the deterministic oracle.

## Canonical input contract

`build_modern_presentation_frame()` deliberately accepts only an exact
`LegacyPresentationConfig::min_screen_width x min_screen_height` surface.
For canonical Mac 1.0.6 this is:

```text
640 x 480 x xRGB1555
```

This restriction prevents a subtle integration error: callers must not first
ask the legacy presenter to center a 640x480 frame inside a host-sized legacy
surface and then send that larger surface through the modern scaler. That
would double-apply legacy centering/borders.

The intended normal-game pipeline is:

1. create/use the canonical 640x480 `LegacyRasterSurface`;
2. run the recovered gameplay-frame/presentation plan into it;
3. call `present_modern_frame()` with the actual native drawable size.

## Upload format

The backend-neutral upload format is tightly packed **RGBA8888**:

```text
R G B A  R G B A ...
```

The old xRGB1555 bit layout is:

```text
xRRRRRGGGGGBBBBB
```

Bit 15 is ignored. Each 5-bit channel is expanded by bit replication:

```text
v8 = (v5 << 3) | (v5 >> 2)
```

Therefore channel endpoints are exact:

```text
0  -> 0
31 -> 255
```

Canonical 640x480 upload properties are:

```text
source width:   640
source height:  480
row bytes:      2560
upload bytes:   1,228,800
```

This conversion occurs only after the recovered renderer has completed the
frame, so none of the established xRGB1555 renderer hashes change.

## Drawable viewport planning

Three host policies are available.

### `AspectFit`

Preserve the canonical 4:3 aspect ratio and maximize coverage without
cropping. Example at 1920x1080:

```text
drawable:  1920x1080
viewport:  x=240 y=0 w=1440 h=1080
```

### `IntegerFit`

Use the largest whole-number scale that fits. Example at 1920x1080:

```text
scale:     2x
viewport:  x=320 y=60 w=1280 h=960
```

If the drawable is smaller than one canonical frame, integer mode falls back
to ordinary aspect-fit so the frame remains visible.

### `Stretch`

Fill the complete drawable and intentionally ignore the source aspect ratio.
This is supported as a host preference but is not the canonical default.

## Sampling policy

`ModernSamplingMode` exposes `Nearest` and `Linear` to host backends.

The dependency-free CPU reference presenter defines **Nearest only**. This is
intentional: exact bilinear sampler results can differ by graphics API,
texture-coordinate convention, and hardware. Linear filtering is therefore a
visual presentation preference rather than a deterministic reference oracle.

For nearest presentation the reference mapping is integer-ratio sampling:

```text
sourceX = sourceWidth  * viewportX / viewportWidth
sourceY = sourceHeight * viewportY / viewportHeight
```

The complete drawable is first filled with `clear_rgba`, then only the planned
viewport is rasterized.

## Backend contract

Platform code implements one small interface:

```cpp
class ModernPresentationBackend {
public:
    virtual bool present(
        const ModernPresentationFrame& frame,
        std::string* error) = 0;
};
```

A backend receives:

- immutable RGBA8888 source pixels;
- canonical source width/height/row bytes;
- physical drawable width/height;
- destination viewport;
- sampling preference;
- clear color.

This is intentionally enough for:

- Metal texture upload + draw + `presentDrawable` on macOS/iPadOS;
- Vulkan staging/upload + textured full-screen quad + swapchain present on
  Linux/Windows;
- D3D upload + swapchain present if a Windows-native D3D backend is later
  preferred;
- test/headless consumers.

The historical
`LegacyPresentationCommit::ImmediateQuickDrawWindowCopyNoSwap` remains
metadata about the original. It does not constrain modern swapchain behavior.

## CPU reference presenter

`rasterize_modern_presentation_reference()` is deliberately dependency-free.
It exists for two purposes:

1. regression-test viewport/letterbox behavior before platform graphics APIs
   are involved;
2. provide a byte-level nearest-neighbour parity oracle for GPU backend tests.

It is not intended to be the shipping renderer.

## Regression contract

`modern_presentation_runtime_test` freezes:

- xRGB1555 -> RGBA8888 channel expansion and bit-15 ignore behavior;
- 1920x1080 aspect-fit geometry `240,0 1440x1080`;
- 1920x1080 integer-fit geometry `320,60 1280x960`;
- small-drawable integer fallback;
- explicit stretch behavior;
- exact 640x480 -> 1,228,800-byte upload size;
- clear-color letterboxing in the CPU oracle;
- nearest integer-ratio sampling witnesses;
- refusal to claim deterministic linear-filter output;
- backend submission/error propagation;
- rejection of already host-sized legacy display surfaces.

The canonical `deimos_reference_probe` additionally locks the modern bridge
against the recovered 640x480 presentation configuration and reports:

```text
modern bridge: RGBA8888 rowBytes=2560 1080pAspectFit=240,0 1440x1080
```

## Next platform work

The next native step should implement the interface without changing the clean
core:

1. **Metal backend** for macOS/iPadOS;
2. **Vulkan backend** for Linux;
3. Vulkan or D3D backend for Windows;
4. cross-backend screenshot/hash tests against the nearest CPU reference
   presenter.

The software xRGB1555 renderer remains available permanently as the canonical
fidelity oracle even after GPU-native presentation is operational.
