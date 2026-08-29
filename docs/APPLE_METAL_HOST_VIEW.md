# Apple Metal Host View

## Purpose

`deimos_apple_host` is the first reusable native-view integration layer above
`deimos_metal_backend`. It creates and owns the platform view (`NSView` on
macOS, `UIView` on iPadOS) and its `CAMetalLayer`, while leaving application
lifecycle, navigation, input, audio, simulation, and resource ownership to the
real app.

The boundary is:

```text
canonical gameplay renderer
        |
        v
640x480 xRGB1555 display surface
        |
        v
AppleMetalHostView::present()
        |
        +-- sync native point size -> physical drawable pixels
        +-- present_modern_frame()
        +-- AppleMetalPresentationBackend
        v
CAMetalLayer / CAMetalDrawable
```

No gameplay behavior is implemented in this target.

## Target

On Apple toolchains with `DEIMOS_BUILD_APPLE_METAL=ON`, CMake now exposes:

```text
deimos_metal_backend
deimos_apple_host
```

`deimos_apple_host` links the Metal backend plus the appropriate native UI
framework:

- macOS: AppKit;
- iPadOS/iOS: UIKit.

The public header remains ordinary C++ and exposes native objects only as
borrowed `void*` handles.

## Native view ownership

`AppleMetalHostView::initialize()` must run on the Apple main UI thread.
It creates one native view and one `CAMetalLayer`:

- macOS uses a private `NSView` subclass whose `makeBackingLayer` returns a
  `CAMetalLayer`;
- iPadOS uses a private `UIView` subclass whose `+layerClass` is
  `CAMetalLayer`.

`native_view_handle()` returns the borrowed `NSView*`/`UIView*` so the shipping
application can insert the view into its own hierarchy. The C++ host object
retains the view for its lifetime.

## Retina / iPad drawable geometry

The host distinguishes logical layout points from physical drawable pixels.
`sync_drawable_geometry()` reads the current native view bounds and platform
scale, then sets:

```text
CAMetalLayer.frame        = view bounds in points
CAMetalLayer.contentsScale = native scale
drawableSize               = round(points * native scale)
```

On iPadOS the scale comes from the owning window screen, falling back to the
main screen before attachment. On macOS it comes from the window's backing
scale, falling back to the main screen.

Applications should call `sync_drawable_geometry()` after attaching the view,
after native layout/resizing, and after display/backing-scale changes.
`present()` performs another synchronization defensively before building the
modern frame packet.

## Presentation

`present()` accepts only a completed canonical `LegacyRasterSurface` plus the
recovered `LegacyPresentationConfig`. It then:

1. synchronizes native drawable geometry;
2. obtains the current physical drawable size;
3. calls `present_modern_frame()` using the configured scaling/sampling policy;
4. submits the immutable packet through `AppleMetalPresentationBackend`.

The default policy remains aspect-fit + nearest filtering. The host can select
other `ModernPresentationOptions` without changing canonical renderer output.

## Threading

All view/layer methods are main-thread-only. The host rejects calls from other
threads instead of silently touching AppKit/UIKit from a worker thread.

The simulation may still run elsewhere in a future application, but transfer
of the completed canonical frame into this host must be synchronized by the
application and presented on the main UI thread for this initial integration.

## Validation

The prior `deimos_metal_backend` milestone compiled successfully with both
macOS and iPadOS Apple toolchains before this host-view layer was added. The
first macOS host compile then caught an Objective-C++ scoping defect: the
private `@interface`/`@implementation` declarations had been placed inside a
C++ namespace. They now remain at true file-global Objective-C++ scope, while
all C++ helpers and `AppleMetalHostView` implementation stay inside `deimos`.

Portable validation also includes `apple_objcxx_layout_test`, which prevents
those declarations from being moved back inside `namespace deimos` even on CI
hosts that do not compile Objective-C++. The public-header regression continues
to prove that no Apple types or dependencies leak into `deimos_core`.

The corrected host now compiles and the macOS smoke app has been visually validated in a real resizable `NSView -> CAMetalLayer -> Metal` window. The diagnostic capture shows correct 32+416+160+32 region placement, aspect-fit letterboxing, sharp nearest sampling, and Retina mapping. The remaining Apple validation is the same host path on iPadOS plus the external-original-data frame described in `ORIGINAL_GAME_FRAME_PREVIEW.md`.

## Native smoke application

An optional `deimos_apple_host_smoke` application target exercises the actual
native window/view/layer/present path without requiring original game assets.
Enable it with:

```text
DEIMOS_BUILD_APPLE_SMOKE_APP=ON
```

The smoke app produces a deterministic 640x480 xRGB1555 diagnostic surface
whose visible vertical regions are the recovered `32 + 416 + 160 + 32`
presentation layout. It uses aspect-fit + nearest sampling and redraws after
native resize/backing-scale changes. White guide lines on each canonical region
boundary make orientation, crop, scaling, and Retina mistakes obvious.

This app remains an integration host rather than the final game shell. It embeds no original assets by default. When user-owned `Game.pak` + `Interface.pak` are discoverable (or locally staged into the generated bundle), it now runs a persistent 30 Hz `OriginalGameFramePreview` session: recovered terrain scroll + score-bar convergence -> gameplay-frame render -> Metal presentation. If those files are unavailable, it falls back to the original static diagnostic surface. See `ORIGINAL_GAME_FRAME_PREVIEW.md` and `APPLE_LIVE_FRAME_LOOP.md`.
