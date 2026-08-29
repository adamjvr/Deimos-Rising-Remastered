# External Original-Data Gameplay Frame Preview

## Purpose

`OriginalGameFramePreview` is the first integration fixture that drives the
recovered visible-frame renderer with **real user-owned Deimos Rising data**
without checking any original assets into the repository or release ZIPs.

It began as a static integration fixture. It now also owns the first persistent
live-frame state used by the Apple host while the full entity/input game loop is
being integrated:

```text
Game.pak + Interface.pak (external, user-owned)
        |
        +-- Level 01 definition
        +-- 480x3600 level background
        +-- canonical 160x480 score bar
        +-- TESM 91-frame small-text atlas
        +-- Player-1 score-bar/player sprite family
        +-- three canonical weapon preview sprites
        |
        v
recovered clean resource decoders
        |
        v
render_legacy_gameplay_frame()
        |
        +-- score-bar pixels
        +-- group0 / terrain / group1 / particles / group2
        +-- 576x480 gameplay source composition
        +-- recovered mode-1 32+416+160+32 presentation
        v
canonical 640x480 xRGB1555 frame
        |
        v
ModernPresentationBackend / Metal
```

No PAK data is copied into `deimos_core` or persisted by this runtime.

## Runtime API

The public class is declared in:

```text
include/deimos/original_game_frame_preview.hpp
```

A caller provides the directory containing:

```text
Game.pak
Interface.pak
```

The default preview selects `le01` / Player 1. It loads only the original-data
families required to produce one representative frame rather than decoding the
entire game sprite corpus.

The produced frame is a real execution of the recovered gameplay-frame
boundary. The fixture remains called a *preview* because entity, weapon, audio,
input and full player-world orchestration are not all bound yet. It now advances
the exact terrain-scroll and score-bar convergence ticks at canonical
`FPS_MaxRate=30`, with persistent render surfaces. The Player-1 score-bar/life
sprite family remains at the recovered solo entry position to exercise the
`play` layer through the shared compositor.

## Apple smoke-app behavior

`deimos_apple_host_smoke` now runs in two modes.

### Original-data mode

When it can locate `Game.pak` and `Interface.pak`, it renders the external-data
preview and presents that canonical frame through `AppleMetalHostView`.

Discovery order includes:

1. runtime environment variable `DEIMOS_ORIGINAL_PAK_DIR`;
2. `Paks/` in the application bundle;
3. on macOS, upward searches for the repository's
   `reference/DR-EVID-002/canonical/Paks` directory.

The window title/log identifies the loaded level rather than silently falling
back.

### Diagnostic fallback

If external PAKs are unavailable or fail validation, the smoke app logs the
reason and retains the existing synthetic checker/rainbow frame. This keeps the
native presentation smoke target redistributable and runnable without original
game data.

## Local-only Apple bundle staging

For iPadOS, the app cannot read the Mac repository path. CMake therefore offers
an optional local-only cache variable when the smoke target is enabled:

```text
-DDEIMOS_ORIGINAL_PAK_DIR=/absolute/path/to/Paks
```

When supplied, the generated Apple project copies only `Game.pak` and
`Interface.pak` into `Resources/Paks` of the **local built smoke app**. The
source tree and checkpoint ZIP remain unchanged. The configure step fails if
the requested local directory is missing either file.

This is a developer/test convenience, not a redistribution mechanism.

## CLI probe

`DEIMOS_BUILD_TOOLS=ON` also builds:

```text
deimos_original_frame_probe
```

Usage:

```bash
./build/deimos_original_frame_probe /path/to/Paks
```

The probe reports:

- level name/ID;
- background ID;
- player sprite family/frame;
- number of loaded sprite groups;
- terrain-copy and score-bar-raster status;
- dimensions of the completed canonical display;
- FNV64 of the complete 640x480 xRGB1555 frame.

The canonical 1.0.6 corpus is now frozen as complete-frame FNV64 oracles:

```text
initial : 0x9e8a7ec73b79b254
tick 1  : 0x44dede08075273f2
tick 30 : 0x51d4a7eec9b0beef
```

The probe verifies all three and reports the canonical 30 FPS rate.

## Asset policy

The class references only paths and decoded runtime objects. No original PAK,
TGA, GIF, sound, application, or archive bytes belong in Git or milestone
archives. See `ASSET_POLICY.md` and `CLEAN_ROOM.md`.

## Next step

The persistent native session now covers terrain scroll, score-bar convergence,
recovered gameplay-frame rendering and modern host presentation. The remaining
live-loop integration order is:

1. real player lifecycle/input;
2. entity world tick/spawn activation;
3. weapon/action production;
4. collision/destruction and particles;
5. audio event ownership.

See `APPLE_LIVE_FRAME_LOOP.md`.
