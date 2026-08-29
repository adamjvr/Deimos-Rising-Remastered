# Native Presentation Runtime — Mac 1.0.6

## Scope

This note closes the geometry and call-order boundary between the reconstructed
software world compositor and the original Mac application's final QuickDraw
copy stage. It does **not** reproduce QuickDraw, DrawSprocket, or Carbon APIs in
the clean core. Instead it records the observable presentation contract so a
modern backend can present the recovered xRGB1555 frame without inheriting the
legacy platform stack.

The principal PPC routines are:

- `0x30BC0` — outer world-frame composition and post-world presentation gate;
- `0xBC60` — mode-0 / generic full-frame presenter;
- `0xBEB0` — mode-1 / normal gameplay presenter;
- `0xAC20` — Rect construction + QuickDraw `CopyBits` helper;
- `0x9E40`, `0xA980`, `0xC2A0` — GWorld activation helpers;
- `0xAE20..0xB51C` — display-manager construction and canonical Rect setup.

## `0x30BC0`: two distinct gates

The recovered outer frame routine has two separate Boolean-style inputs that
must not be conflated:

1. its second argument gates the persistent-terrain viewport copy (`0x10120`)
   and particle raster (`0x43BA0`), while the three sprite queue flushes still
   execute;
2. its third argument gates the post-world presentation dispatch at
   `0x30D8C..0x30DCC`.

The closed world order remains:

```text
0x18B20(group 0)
0x10120                  if world-draw gate
0x18B20(group 1)
0x43BA0                  if world-draw gate
0x18B20(group 2)
... timing bookkeeping ...
0xBC60 / 0xBEB0          if presentation gate
```

The mode byte is read from frame object `+0x04`:

- mode `0` -> `0xBC60`;
- mode `1` -> `0xBEB0`;
- any other value -> no presentation call.

`0x30210` stores its third constructor argument into this mode byte. The normal
gameplay caller at `0x56AC` supplies `1`, proving that `0xBEB0` is the ordinary
game presenter. A separate non-gameplay caller at `0x2E554` supplies `0`,
selecting `0xBC60`.

## Canonical `Game[gafl]` presentation contract

The original display manager reads the following positional float-list values.
The clean compiler requires both index and label identity before accepting the
contract:

| Index | Label | Canonical value |
|---:|---|---:|
| 52 | `MinScreenWidth` | 640 |
| 53 | `MinScreenHeight` | 480 |
| 54 | `VisibleGameWidth` | 416 |
| 55 | `VisibleGameHeight` | 480 |
| 56 | `ReqDisplayDepth` | 16 |
| 57 | `ScoreBarWidth` | 160 |
| 58 | `ScoreBarHeight` | 480 |
| 59 | `LeftBorderWidth` | 32 |
| 60 | `RightBorderWidth` | 32 |

The width identity is exact:

```text
32 + 416 + 160 + 32 = 640
```

This is important architectural evidence: the 416-pixel gameplay viewport is
not the complete presentation frame. The original frame reserves a distinct
160-pixel score-bar region plus 32-pixel borders on each side.

## Display-manager Rect geometry

`0xAE20..0xB51C` centers the minimum 640x480 presentation frame within the
physical display. For physical size `(W,H)`:

```text
frameLeft = (W - 640) / 2
frameTop  = (H - 480) / 2
```

Normal gameplay destinations are then:

```text
left border : [frameLeft,      frameLeft + 32)
game        : [frameLeft + 32, frameLeft + 448)
score bar   : [frameLeft +448, frameLeft + 608)
right border: [frameLeft +608, frameLeft + 640)
```

All four regions are 480 pixels high. The normal initialized path clears the
optional vertical-adjust flag, so no extra Y displacement is applied.

At exactly 640x480 this becomes:

```text
x=0..31     left border
x=32..447   416-pixel game
x=448..607  160-pixel score bar
x=608..639  right border
```

At 800x600 the 640x480 frame is centered at `(80,60)`, placing gameplay at
`x=112..527` and score bar at `x=528..687`.

## Mode 0 — `0xBC60`

The generic presenter constructs a single 640x480 source Rect and copies it to
the centered minimum frame using `0xAC20` / QuickDraw `CopyBits`.

Conceptually:

```text
source      {top=0, left=0, bottom=480, right=640}
destination {top=frameTop, left=frameLeft,
             bottom=frameTop+480, right=frameLeft+640}
```

This path is selected by the confirmed non-gameplay mode-0 caller.

## Mode 1 — `0xBEB0`

Normal gameplay uses two `CopyBits` operations from the presentation source
canvas:

```text
game source:
  {0, 0, 480, 416}

game destination:
  {frameTop, frameLeft+32, frameTop+480, frameLeft+448}

score-bar source:
  {0, 416, 480, 576}

score-bar destination:
  {frameTop, frameLeft+448, frameTop+480, frameLeft+608}
```

Thus the source canvas contains the game region immediately followed by the
score-bar region. The score bar is **already populated before this routine**;
`0xBEB0` is a presenter/copy routine, not the score-bar renderer.

When the host display is wider than the minimum width, display-manager byte
`+0x64` causes `0xBEB0` to set QuickDraw foreground color `33` (black) and
`PaintRect` the two 32-pixel side-border strips before the copies. At exactly
640 pixels wide this repaint path is not taken.

Byte `+0x65` provides an early-return suppression path. The clean planner
models the ordinary enabled presentation path and leaves platform/window state
outside the portable core.

## QuickDraw evidence

The imported application calls observed in this boundary include:

- `InterfaceLib::SetGWorld` (`0xD492C`);
- `InterfaceLib::SetRect` (`0xD3A14`);
- `InterfaceLib::PaintRect` (`0xD4D94`);
- `InterfaceLib::ForeColor` (`0xD558C`);
- `InterfaceLib::CopyBits` (`0xD5664`);
- `InterfaceLib::GetMainDevice` (`0xD5454`).

`0xAC20` converts the stored 32-bit Rect representation to QuickDraw Rects,
obtains the destination GWorld, and calls `CopyBits` with copy mode 0 and no
mask region.

A deeper trace corrects one tempting misclassification: `0x9E40` is only a
GWorld activation helper. It dereferences the supplied wrapper and calls
`SetGWorld`; it does not draw score-bar UI.

## DrawSprocket ownership and final commit semantics

The final display path is now closed far enough to eliminate a second tempting
modern assumption: **the original does not use a DrawSprocket back-buffer swap**.

The PEF imports the following DrawSprocket display functions:

- `DSpContext_GetState`;
- `DSpContext_SetState`;
- `DSpProcessEvent`;
- `DSpGetVersion`;
- `DSpShutdown` / `DSpStartup`;
- `DSpSetBlankingColor`;
- `DSpContext_Release`;
- `DSpContext_GetFrontBuffer`;
- `DSpContext_Reserve`;
- `DSpFindBestContext`.

It imports **neither** `DSpContext_GetBackBuffer` nor
`DSpContext_SwapBuffers`.

`0xC470` is the DrawSprocket context setup path used by `0xAE20` when the
fullscreen context is selected. After reserve/state setup, `0xC81C` makes the
single recovered `DSpContext_GetFrontBuffer` call. The returned GWorld is
passed to `0x44B50`, whose complete body only reads its QuickDraw bounds
(shorts at `+0x10..+0x16`) into the caller-supplied rectangle. The returned
front-buffer pointer is not retained as the `CopyBits` destination.

Back in `0xAE20`, those recovered display bounds are passed to `0xA640`.
`0xA640` calls the imported `NewCWindow` and stores that window pointer in the
display manager's GWorld/window wrapper at `+0x04`. `0xC2A0` activates that
wrapper via `0xA980` -> `SetGWorld`, and `0xAC20` passes its embedded QuickDraw
port bitmap to `CopyBits`.

Therefore the original 1.0.6 commit chain is:

```text
DrawSprocket context reserve/activate
        |
        +-- GetFrontBuffer once -> discover physical display bounds
        |
        +-- NewCWindow matching those bounds
                |
                +-- SetGWorld(window port)
                +-- PaintRect / CopyBits from composed source canvas
                +-- no GetBackBuffer
                +-- no SwapBuffers
```

The observable legacy commit is consequently an **immediate QuickDraw window
copy with no explicit DrawSprocket flip after `0xBEB0`/`0xBC60`**. This is a
statement about the original Mac implementation, not a restriction on the
remaster: a Metal/Vulkan/OpenGL/native backend can and should use its own
appropriate swapchain/present primitive after mapping the recovered plan.

## Clean-core implementation

`presentation_runtime.hpp/.cpp` provides:

- `LegacyPresentationConfig` and label-verified compilation from
  `Game[gafl]` 52..60;
- `LegacyPresentationMode::{FullFrame,Gameplay}`;
- `LegacyPresentationCommit::ImmediateQuickDrawWindowCopyNoSwap`, preserving
  the original commit semantics without constraining modern backends;
- `plan_legacy_post_world_presentation()` for the recovered Rect/copy/clear
  contract;
- `execute_legacy_presentation_plan()` as a bounded portable xRGB1555
  reference implementation.

The clean legacy implementation intentionally rejects host surfaces smaller than
the recovered 640x480 minimum rather than reproducing undefined legacy platform
behavior. `modern_presentation_runtime` now provides the next boundary: the exact
640x480 xRGB1555 result is converted after raster completion to RGBA8888 and
submitted through a backend-neutral host interface. See
`MODERN_PRESENTATION_RUNTIME.md`.

## Regression contract

`presentation_runtime_test` freezes:

- exact 52..60 label/value compilation;
- mode-0 centered 640x480 copy;
- mode-1 game/score-bar source and destination Rects at 640x480;
- centered mode-1 geometry and side-border clears at 800x600;
- the independent presentation-enable gate;
- unknown-mode fallthrough with no copy;
- portable pixel execution with distinct game/score/border/outside witnesses;
- malformed positional labels rejected.

The canonical `deimos_reference_probe` additionally requires:

```text
native presentation frame: 640x480x16 = 32 + 416 + 160 + 32
```

All established renderer, gameplay, and RNG oracles remain unchanged.

## Boundary now closed / next platform targets

Closed by the presentation milestones:

- the post-`0x30BC0` mode dispatch;
- normal-game mode identity;
- minimum frame dimensions and score-bar packing;
- exact `CopyBits`/border Rect geometry;
- original score-bar pixel production;
- final no-swap QuickDraw commit semantics;
- a backend-neutral 640x480 xRGB1555 -> RGBA8888 host seam with tested
  letterbox/scaling geometry.

Next platform targets:

1. implement a Metal `ModernPresentationBackend` for macOS/iPadOS;
2. implement a Vulkan backend for Linux;
3. establish screenshot parity against the dependency-free nearest reference
   presenter before moving additional rendering work to the GPU;
4. keep any remaining non-gameplay/front-end producers above the same canonical
   frame boundary rather than bypassing it.
