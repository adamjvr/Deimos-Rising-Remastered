# Apple Metal Presentation Backend

## Purpose

`deimos_metal_backend` is the first native shipping-path adapter built on top
of the backend-neutral modern presentation seam. It targets both macOS and
iPadOS and deliberately lives outside `deimos_core`.

The fidelity boundary remains unchanged:

```text
canonical simulation + software renderer
             |
             v
640x480 xRGB1555 legacy frame
             |
             v
modern_presentation_runtime
  immutable RGBA8888 packet + viewport
             |
             v
deimos_metal_backend
             |
             v
CAMetalLayer / CAMetalDrawable
```

The Metal backend never owns gameplay state, particle state, score-bar state,
terrain state, sprite queues, or canonical xRGB1555 pixels.

## Build target

CMake exposes:

```text
DEIMOS_BUILD_APPLE_METAL=ON
```

The option defaults to `ON`, but `deimos_metal_backend` is created only when
`APPLE` is true. Linux and Windows therefore do not enable Objective-C++ or
link Apple frameworks merely because the source exists in the repository.

The target links:

- `deimos_core`;
- Metal;
- QuartzCore;
- Foundation.

The Objective-C++ source is compiled under ARC.

## Host ownership

The native application owns the `CAMetalLayer`. To keep the public C++ header
free of Objective-C declarations, the backend accepts the layer as `void*`.
The Objective-C++ implementation bridges that handle back to `CAMetalLayer*`
and retains it for the backend lifetime.

A host can query `drawable_size()` before calling `present_modern_frame()` so
the bridge packet is always planned against the current physical drawable
size. If the layer resizes after a packet was built, `present()` rejects that
stale packet instead of stretching it implicitly.

## Per-frame behavior

For each valid `ModernPresentationFrame`, the backend:

1. verifies the `CAMetalLayer`, Metal device, command queue, samplers, and
   render pipeline;
2. uploads the tightly packed canonical RGBA8888 frame into one
   `MTLPixelFormatRGBA8Unorm` texture;
3. obtains the next `CAMetalDrawable`;
4. clears the entire drawable to `frame.clear_rgba`;
5. applies `frame.viewport` as the Metal viewport;
6. draws one triangle-strip textured quad;
7. chooses nearest or linear filtering from `frame.sampling`;
8. calls `presentDrawable` and commits the command buffer.

There is no gameplay logic and no second scaling calculation in the backend.
The host-independent `ModernViewport` remains authoritative.

## Shader contract

The initial backend compiles a tiny Metal shader library once per device/pixel
format. The vertex shader emits a four-vertex full-screen quad and top-left
oriented texture coordinates. The fragment shader performs one texture
sample. No color grading, gamma correction, post-processing, blending, or
other fidelity-changing work occurs in this layer.

The runtime shader source is intentionally small for the first integration
milestone. A later app-packaging pass may replace it with an offline-built
`.metallib` without changing the `ModernPresentationBackend` contract.

## Sampling

Nearest filtering is the canonical presentation mode and should be used for
cross-backend screenshot parity. Linear filtering remains an optional visual
host preference and is not a deterministic oracle.

## Resize and lifecycle rules

- The application owns window/view resizing.
- The application updates `CAMetalLayer.drawableSize` in physical pixels.
- The application queries `drawable_size()` and rebuilds the modern frame for
  that exact size.
- A zero drawable size means presentation should be skipped.
- Replacing the layer invalidates cached Metal device resources.
- Changing the layer pixel format rebuilds the render pipeline.
- Changing canonical source dimensions recreates the upload texture.

## Validation status

Linux CI/local testing validates that the new public header remains ordinary
portable C++ and that adding the Apple target does not perturb `deimos_core`.
The complete existing deterministic suite and canonical PAK probes remain the
reference gate.

The Objective-C++ Metal target has now compiled successfully with both macOS
and iPadOS Apple toolchains. The next validation layer is the reusable native
host view plus real drawable/screenshot parity; see `APPLE_METAL_HOST_VIEW.md`.

## Next Apple step

The static backend has compiled on both Apple targets. `deimos_apple_host` now
implements the minimal reusable native view layer above it. The next validation
step is to build that target, insert its native view into macOS/iPadOS app view
hierarchies, present a canonical frame, and compare nearest-mode captures to the
CPU reference presenter.
