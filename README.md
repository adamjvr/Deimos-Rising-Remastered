# Deimos Rising Remastered

Evidence-driven, clean-code reconstruction and native remaster of **Deimos Rising**.

## Evidence baseline

The working corpus includes an older Mac distribution, a fully recovered Mac **1.0.6** installation, a Windows PE32/NSIS distribution, and an add-ons/update/reference/mod/music corpus. Phase 0 remains open for additional evidence, while active development is in Phase 1 with the first binary-confirmed Phase-2 state-runtime primitives now implemented.

## Reconstruction rule

**Game executable logic is independently reconstructed. Original supplied art/audio/data are intentionally used as the canonical asset baseline until restored/upscaled replacements are produced.** Exact resource identities remain stable so restored content can override originals one asset at a time. See `docs/CLEAN_ROOM.md` and `docs/ASSET_POLICY.md`.

## Clean core status

The portable C++20 core now contains:

- exact four-byte resource/FourCC naming;
- IA/IC alpha/color plate parsing;
- a dependency-free reader for the original stored-ZIP PAKs with CRC32 validation;
- `Data/Local`-over-PAK lookup;
- the recovered legacy seven-bit tagged-text decoder/parser;
- typed scalar/FourCC/RECT/RGB helpers;
- a strict level loader validated against all 12 original levels;
- typed ID/float/color/rect/string-table and Text Format loaders;
- a conservative v10005 replay parser for fields proven from the corpus;
- typed unit/weapon/player definitions with explicit states, spawn sets, rules, and weapon spawns;
- a unit-behavior compiler with the complete 17-condition PPC dispatch vocabulary;
- exact first-match rule execution and exact/case-sensitive state-action resolution;
- binary-confirmed timer, state-entry-counter, and range-transition runtime primitives;
- binary-confirmed spawn scheduler, target terrain-effect gate, rotated/absolute/relative geometry, and full constructor-request handoff;
- headless normal group/member construction with original group/appearance RNG, owner/parent identity, serials, initial position/speed/heading, state-zero entry, spawn-runtime initialization, and cumulative group delay;
- recovered original 360-entry trig-table construction contract;
- cross-resource reference validation;
- a PEF packed-data/import/relocation probe that resolves the original main transition vector and TOC;
- a `deimos_reference_probe` executable that validates an original `Game.pak` through the clean implementation.
- binary-confirmed persistent terrain/background camera runtime: 416x480x16 source-view configuration, 545-row activation prime, exact vertical scroll/clamp/end behavior, and full-viewport copies from the persistent terrain raster;
- binary-confirmed particle raster and outer world-frame composition: exact 7x7 xRGB1555 particle kernel plus `group0 -> terrain copy -> group1 -> particles -> group2` ordering with the original draw-latch gates;
- binary-confirmed native presentation path: 640x480x16 minimum frame, normal gameplay `32 + 416 + 160 + 32` layout, exact game/score-bar QuickDraw copy rectangles, centered placement, and the original immediate CWindow `CopyBits` commit with no DrawSprocket back-buffer/swap import;
- binary-confirmed score-bar producer/cache runtime: label-verified 160x480 dirty regions and coordinates, exact shield/power smoothing, player/weapon score-bar resources, displayed-life semantics, and upstream score/extra-life threshold production;
- original-pixel score-bar renderer: canonical 160x480 `scor` TGA, `Interface.pak` `tesm` 91-frame small-text atlas, exact `%0.7i` score / `%i` lives formatting, cyan-to-red last-life style, dirty background restoration, meter COST masks, weapon previews, and score-bar pixel FNV64 `0xd2f48984985f54d8`;
- complete portable normal-gameplay frame orchestration: recovered `score-bar draw -> world composition -> 576x480 source composition -> mode-1 presentation` order with independent world/presentation gates;
- binary-confirmed level-selection acceptance/failure pulse runtime: `lsca`/`lscf` color+blend styles, 0.18/0.25 scale rates, 2.0 maxima, exact fade/ping-pong lifecycle, and shared `COST` rectangle request generation;
- first modern host-presentation seam: canonical 640x480 xRGB1555 frames convert only after deterministic raster completion to RGBA8888, with aspect-fit/integer-fit/stretch viewport planning, a platform-backend interface, and a dependency-free nearest-neighbour CPU parity oracle.
- first native adapter: Apple-only `deimos_metal_backend` for macOS/iPadOS, using one RGBA8 upload texture, the backend-neutral viewport, selectable nearest/linear sampling, and one drawable commit while keeping `deimos_core` free of Apple APIs; the backend has now compiled successfully on both macOS and iPadOS toolchains;
- first reusable Apple host view: `deimos_apple_host` creates/owns an `NSView` or `UIView` backed by `CAMetalLayer`, tracks logical-point to physical-pixel Retina/iPad sizing, exposes the borrowed native view for app hierarchy integration, and presents completed canonical 640x480 frames through the backend-neutral bridge;
- first original-data live native session: `OriginalGameFramePreview` persists terrain/HUD/display state and advances recovered terrain scroll plus score-bar convergence at canonical `FPS_MaxRate=30`; the Apple host continuously presents those frames through Metal. Complete-frame no-input oracles are initial `0x9e8a7ec73b79b254`, tick 1 `0x44dede08075273f2`, and tick 30 `0x51d4a7eec9b0beef`.
- first bounded player-control integration: canonical Player-1 tuning (`maxSpeed=7.8`, `velocityDelta=1.6`) plus label-verified `Player_TopGameAreaLimit=13` drive a portable preview-control snapshot; the macOS live host maps arrows/WASD to it. One right-input tick is frozen at player `(209.6,330)`, velocity `(1.6,0)`, full-frame FNV64 `0x6fd5c94a64dcb0c8`. This remains explicitly separate from unresolved original InputSprocket/film-bit semantics.
- historical first live-world integration checkpoint: Level-1 placement activation follows the recovered terrain-row scheduler instead of instantiating the entire level at boot; only the two placements reached by the initial `sourceTop-64` row stream are live initially, with the third placement activating at tick 36. All five Weapon Definitions compile into a level-aware player weapon catalog, PlayerWorld slot 0 is authoritative, state rules receive original-shaped Unit-ID/range/global world facts, and the recovered collision -> destruction-effects -> outer-removal chain is connected in the 30 Hz session. The original integration witnesses are retained in `docs/LIVE_WORLD_WEAPON_INTEGRATION.md` as historical checkpoints; current WIP8 live hashes are listed below.
- native macOS host smoke is now visually validated end-to-end; `OriginalGameFramePreview` can additionally load user-owned `Game.pak` + `Interface.pak` at runtime and feed a real Level-1 background/score-bar/player-sprite frame through `render_legacy_gameplay_frame()` without embedding original assets, with `deimos_original_frame_probe` available to freeze the resulting whole-frame FNV64.
- WIP8 live animation/orientation + main-tick ordering closure: recovered state animation fields now drive directional initialization, finite stop, same-tick `Animation Has Stopped` rules, and visual-only RotateToTarget; delayed members use the recovered `1 -> 0` same-tick gate; movement/lifetime, owner/spawn, ground-obstacle, and following-frame vertical-scroll-pause ordering are frozen by focused tests and real-data probes. Static frame witnesses remain unchanged; current live witnesses are `0xcd72678207b195b7`, `0x800f06651d29406a`, and `0x267609db3ba6dbcc`, with playable dying@171 / respawn@252 and a bounded 3,000-tick stress world. See `docs/WIP8_ANIMATION_AI_ORDERING.md`.
- WIP9 target/flee + enemy-fire-heading closure: the canonical 1.0.6 PEF was recovered again from the original StuffIt/SMI evidence chain and used to close `stateFlee_ID`, no-active-player flee, range-transition flee timing, and PPC `0x161C0` live visual heading. Flee now accelerates toward authored destinations instead of away from the player, and rotation-adjusted state spawns use current sprite direction rather than stale construction heading. Re-audit of `0x15B40`/`0x17CB0` confirms enemy firing cadence remains the existing instruction-closed spawn scheduler. WIP8 static/live tick-120 hashes and dying@171 / respawn@252 remain unchanged; the corrected 3000-tick profile is maxResident=84 / finalResident=15 / pruned=1773 / farCulled=136. See `docs/WIP9_FLEE_TARGET_FIRE_HEADING.md`.


The original 1.0.6 `Game.pak` has been loaded directly through this clean code: all 763 files CRC-validate, all 12 levels parse, all 565 level placements reconcile, and all four canonical PAK films parse, and all 386 unit definitions / 5 weapons / 2 player definitions parse and cross-validate. Across all four original PAKs, **871 actual files** pass CRC validation.

## Build tests

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build -j
ctest --test-dir build --output-on-failure

# Optional: validate your local canonical reference PAK directly
./build/deimos_reference_probe reference/DR-EVID-002/canonical/Paks/Game.pak

# Optional: verify the external-original-data Level-1 initial + live-tick frame oracles
./build/deimos_original_frame_probe reference/DR-EVID-002/canonical/Paks

# Optional: verify crash/respawn, Plasma Bomb damage, Ion Cannon charge/release, and 3000-tick stress
./build/deimos_playable_runtime_probe reference/DR-EVID-002/canonical/Paks
```

Committed tests use synthetic fixtures only; the current suite passes 53/53. Original assets remain in the ignored local reference workspace.

See `docs/STATUS.md`, `docs/ROADMAP.md`, and `reverse/formats/` for current reconstruction details.

## Current clean-world runtime milestone

The portable core now includes a handle+serial `EntityWorld` registry and the
binary-confirmed owner-location state layer. Canonical 1.0.6 contains 156
Lock-to-owner states, 10 Link-to-owner states, and 8 Orbit-owner states. The
world-aware entity tick executes those modes in the recovered post-range,
pre-spawn-scheduler slot. See `reverse/formats/ENTITY_WORLD_RUNTIME.md`.
