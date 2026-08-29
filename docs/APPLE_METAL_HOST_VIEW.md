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

The next Apple validation gate is therefore:

1. compile corrected `deimos_apple_host` on macOS and iPadOS;
2. build/run `deimos_apple_host_smoke`;
3. present the deterministic 640x480 diagnostic frame in a real native view;
4. capture nearest-mode output and compare it against the CPU reference
   presenter for the same physical drawable size.

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

This app is intentionally a presentation smoke test rather than a game shell.
It contains no original game assets and no gameplay simulation. Once this path
is visually validated, the same `AppleMetalHostView::present()` call becomes
the destination for the real `render_legacy_gameplay_frame()` output.
