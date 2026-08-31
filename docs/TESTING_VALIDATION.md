# Testing and Validation

## Evidence integrity

Every supplied archive receives stable MD5/SHA-1/SHA-256 identity before transformation. Extracted containers, executables, resource forks, PAKs, and important derived payloads receive independent hashes. Version/build relationships are established by internal evidence where possible rather than filenames alone.

## Resource-system tests

Clean code must preserve four-character tags byte-for-byte, including case and spaces. IA/IC plate parsing is tested independently of image decoding. Local-vs-PAK precedence will receive explicit tests before it is relied upon by gameplay code.

## Serialization tests

Recovered game-data formats are not accepted from guessed C structs. Each parser must have:

- bounds-checked reads;
- field-by-field evidence notes;
- synthetic fixtures;
- tests for malformed/truncated data;
- round-trip tests where serialization is reconstructed;
- cross-file reference validation against known four-character tags.

## Replay/behavior validation

The corpus contains four canonical built-in `.film` resources and eleven additional Perfect Demos recordings. Once the film format is decoded, these recordings become high-value deterministic regression inputs for:

- player motion and firing;
- enemy/unit sequencing;
- collision/damage;
- level timing/scrolling;
- weapons/projectiles;
- scoring and pickups;
- RNG/timing behavior where encoded or inferable.

Recorded reference outcomes are compared to the clean simulation. Discrepancies are logged rather than normalized away.

### State-runtime regression rules

Binary-confirmed transition behavior receives explicit synthetic tests for:

- exact/case-sensitive state-name lookup;
- all 17 supported rule-condition classifications;
- first-true-rule ordering, including unresolved/no-op actions;
- the zero-range behavior of within/not-within-player conditions;
- exact float equality for visibility/tint/scale predicates;
- the original 32-bit LCG and 15-bit result sequence;
- inclusive integer range mapping and equal-bound no-consume behavior;
- exact timer-tick equality;
- 20-slot state-entry counters and immediate counter threshold checks;
- strict `<` range-transition comparison.

Real `Game.pak` validation is then rerun to detect any parser/compiler drift across all 386 units and 5,835 rule slots.

### Collision/damage regression rules

The binary-confirmed collision layer has a dedicated synthetic regression executable covering:

- PPC `fctiwz` AABB edge construction and touching-edge overlap;
- collision domain, opposite harmless-class, active/participation/group-delay gates;
- asymmetric player-projectile candidate policy and offscreen behavior boundary;
- exact `0x42F80` radial rejection/acceptance, including strict entity/entity equality and quantized squared-distance behavior;
- player half-integer radius behavior and the 6.5-distance quantization edge case;
- player-collision viewport boundaries, two-slot snapshot/order, owner redirection, reciprocal damage, status recheck, and pickup success/failure exclusivity;
- strict `Entity_HitDelay` timing and shield clamp/destruction;
- collision-invulnerability shield restoration;
- strict delayed on-hit state change;
- preservation of pre-hit-state glow/spawn fields after same-call state transition;
- collision-spawn equality boundary and non-repeat bookkeeping;
- both `passHitsToOwner` legs, including the Mac 1.0.6 second-leg self-parent quirk;
- scan early exit after self becomes inactive;
- level-scaled shield base/increment/max behavior, including the no-max-clamp branch when increment is non-positive.

The repository suite is currently **53/53 PASS**. The suite includes the label-verified native presentation planner/executor contract, score-bar producer/cache/score-threshold contract, and isolated level-selection acceptance/failure pulse; the canonical probe reports `native presentation frame: 640x480x16 = 32 + 416 + 160 + 32`. The optional canonical `Game.pak` probe additionally validates all compiled collision/destruction/terrain-media/player-runtime fields and reports 436 collision-enabled states, 135 air / 251 ground Unit Definitions, 8 pickup Unit Definitions (4 coin / 1 mult / 1 exli / 2 shie), 2 Player Definitions, player globals 100/10/1, death-money IDs `calg/cals/casg/cass`, 67 shadow casters, 4 ground-obstacle colliders, 12 any-media death spawners, 3 non-`none` media-impact units, and fixed water IDs `spti/spsm/spme/spla`, **0** stock
`stateUseThisStateOnShieldDepletion_BOOL` states / affected units, plus the other corpus counts recorded in `STATUS.md`. It also verifies that the existing shared constructor and first player-aware tick remain deterministic at RNG seeds `2249411936` and `2633739833` respectively.

### Player-runtime regression rules

The binary-confirmed player runtime has a dedicated synthetic regression executable covering:

- Player Definition source-to-compiled shield/life/resource fields;
- label-verified `Game[gafl]` 161/162/167 positional globals and PPC integer truncation;
- label-verified `Objects[gaob]` 2..5 death-money resources;
- zero/nonzero coin behavior and semantic money mutation;
- exact multiplier ladder;
- extra-life cap/spawn and shield pickup clamp;
- retained `air `/`grnd` invulnerability rejection;
- player hit-delay write ordering relative to invulnerability;
- direct shield-damage scaling, zero-shield survival and strict negative-shield death;
- hit-spawn and low-shield warning gates/latches;
- immediate death status/invulnerability/money decomposition without an incorrect life decrement;
- ground-obstacle stop preserving position while zeroing velocity and latching stationary;
- live `+0x19` air-domain exclusion before ground-obstacle collision;
- live `+0xCD` shield-depletion-state routing through first marked state `0x17E70`, with ordinary destruction suppressed;
- PlayerDef lifecycle offset compilation for game-over/dying/final-dying/entry timing and solo/multi entry coordinates;
- strict status-2 entry-delay equality boundary and respawn entry spawn;
- respawn velocity from the executable's relocated `{0,0}` literal;
- ordinary versus final-life dying timers and gameplay-start-gated life decrement;
- status-1 game-over countdown/disable;
- strict status-4 entry-invulnerability expiry plus external/live blocker gates.


### Visual/render-request regression rules

The renderer-boundary regression executable covers:

- Unit/state visual source fields and the recovered compiled offsets;
- initial visibility/tint/scale setup plus shared-RNG scale tolerance;
- exact `0x12750` visibility/tint convergence and `0x12840` scale convergence/clamping behavior;
- geometry-dirty behavior for actual scale movement and sprite face/frame changes;
- main layer mapping including zero/`none` normalization and corrected `plwe`/`play`;
- the separate shadow-layer mapping;
- shadow-before-main `0x12F20` request order and temporary pass selection;
- base/tint/collision-glow request ordering and `stateDoColorise_BOOL` base suppression;
- terrain-draw bypass of ordinary layer numbering and HUD world-space handling.

The canonical `Game.pak` probe additionally checks every newly compiled visual field against parsed source data. Current canonical coverage is 17 scale-tolerance Unit Definitions, 62 colorise states, 2 terrain-draw states, 111 nonzero-tint states, 584 non-100 visibility states, and 506 non-100 scale states. The raw layer distribution is `defa=156, grou=17, grhi=68, ailo=10, aihi=51, plwe=5, play=0, plsh=2, plef=0, plui=10, atmo=0, hud=17, none=50`.

### Sprite-resource regression rules

The sprite-resource regression and canonical probe cover:

- indexed GIF87a/89a decoding in the palette-index domain consumed by the Mac alpha-plate scanner;
- exact marker-row/marker-column discovery and per-cell corner-value trimming from `0x1F140..0x1F5B0`;
- source-rectangle ordering and deliberately variable frame bounds;
- atomic loaded-group publication, absent-group behavior, frame count, and high-frame fallback to frame zero;
- PPC `fctiwz` scaled dimensions and `0x19CA0` lazy-load retry;
- `0x12940` width/height and signed half-extent refresh, including the stale width/height behavior for a `none` face;
- explicit safe rejection of negative frame indices instead of reproducing the original unsafe pre-array access.

Canonical corpus validation decodes all 124 alpha plates, extracts 2,463 alpha-frame rectangles, confirms 123 existing alpha/color pairs have matching plate dimensions, and records `PDLI` as the stock alpha-only exception. Sample identities are bound as `PL1B=7` frames with 53x43 frame zero, `EXLG=12`, `BOCR=3`, and `GLOW=12`.

### Destruction/group-removal regression rules

The binary-confirmed destruction layer has a dedicated synthetic regression executable covering:

- exact `0x16300` consequence ordering and idempotence when collision already processed destruction effects;
- destruction spawn, particles/color, notice tick, and complete sound-descriptor capture;
- `FourCC{}` plus serialized `none`/`NULL` resource-sentinel handling;
- child destruction versus deletion using serial-only parent matching and the per-state opt-in flags;
- separate original/active/destroyed group counters and group-kill detection from destroyed-count equality;
- player-attributed ordinary/group-kill coin rewards and consumed-pickup suppression;
- the special `SERM` group-removal exemption;
- owner destruction propagation, deletion spawns, obstacle requests, and terrain-draw requests in outer cleanup order;
- canonical random-bonus threshold selection, the pending ground-accuracy reward branch, progression gating, and resource-table label validation;
- legacy float-to-integer truncation when compiling random-bonus percentage resources.

- exact `0x43BA0` 7x7 particle raster: label-bound tuning, clip contract, radial transparency/core-color topology, center discontinuity, view offset, and delay gate.
- exact `0x30BC0` world-frame precedence and draw latch: group 0 -> terrain copy -> group 1 -> particle raster -> group 2.

The optional canonical `Game.pak` probe additionally checks every newly compiled destruction field against the source-format Unit Definition/state values and verifies the fixed random-bonus positional contracts. Canonical destruction coverage currently reports 99 destruction-spawn units, 99 particle units, 77 destruction sounds, 28 ordinary coin-reward units, 15 group-kill reward units, 13 obstacle creators, 32 terrain-draw units, and 7 random-bonus units.

## Platform parity

Portable-core tests run identically on macOS, iPadOS host-compatible test targets where practical, Linux, and Windows. Rendering/input/audio adapters may differ, but gameplay/resource semantics must not.


## Audio/music corpus validation

`audio_resource_test` binds a synthetic stereo AIFC/IMA4 resource, 80-bit sample-rate parsing, 34-byte packet decoding, and tolerant handling of an under-declared FORM size.

When `Audio.pak` and `Music.pak` are present beside the canonical `Game.pak`, `deimos_reference_probe` additionally verifies:

- 96/96 Audio.pak effects decode as mono 44.1 kHz IMA4;
- total decoded SFX frames = 3,133,376;
- all three Music.pak resources decode as stereo 44.1 kHz IMA4;
- `mu03`: 8,633,088 PCM frames, little-endian s16 CRC32 `4f945e4e`;
- `ammu`: 1,533,824 PCM frames, CRC32 `9871dd60`;
- `inmu`: 2,633,792 PCM frames, CRC32 `60d31157`.

The three music checksums were established against an independent FFmpeg decode. The clean QuickTime predictor-continuity implementation then reproduces that PCM sample-for-sample.

### Sprite frame / shadow canonical oracle

The sprite-resource probe now goes beyond atlas rectangles and rebuilds every normal paired frame surface. Canonical Mac 1.0.6 results are:

```text
paired frame surfaces:            2460
frames with transparency plane:   2460
color words:                   3115564
transparency words:            3115564
row-skip sentinels:               6341
sprite-surface FNV64: 0x9f9dcfba05b5089c
shadow offsets air/ground: -48,104 / -6,8
```

`tests/sprite_frame_bitmap_test.cpp` binds xRGB1555 construction, transparent-key fallback, mask weights, plane omission, and the row-1000 sentinel. `tests/shadow_runtime_test.cpp` binds air/ground scale and layer selection, fixed-vs-scaled air offsets, horizontal-view shifting, terrain submission, and legacy shadow transparency.

`tests/render_orchestration_test.cpp` binds the recovered semantic-to-raw request bridge end-to-end: cached-frame resolution, shadow/base/tint/glow ordering, exact world/HUD coordinates, distinct main-terrain `+32` and terrain-shadow `-32` X bases, layer/flag/effect packing, immediate-vs-queued submission, terrain sequence stamping, compositor handoff, and the `0x100B0` horizontal-view step/clamp/direction-latch behavior.



### Terrain surface/camera regression rules

`terrain_surface_runtime_test` executes the recovered `0xFA10/0xFA90/0x10000/0x10120/0x10220` contracts with assertions enabled and covers the label-verified 416x480x16 configuration, bottom-most +32 source crop, all 545 initial row callbacks, row suppression, persistent terrain mutation lifetime, full-viewport copies across horizontal offsets -32/0/+31, exact vertical applied-delta accounting, top-64 row activation, zero-delta behavior, top/bottom clamps, and the executable's normal source-top==1 end-scroll quirk.

The canonical `Game.pak` probe now independently verifies `terrain viewport/depth: 416x480x16` without changing the shared constructor/first-tick oracles (`386 / 546 / 544`, seeds `2249411936 / 2633739833`).


### Particle lifecycle / producer regression rules

`particle_lifecycle_test` freezes the recovered `0x44630/0x431F0/0x43340/0x438C0` contract: the 302-draw startup oracle, preset mapping, cursor-99 quirk, unknown-preset no-RNG behavior, RGB24-to-xRGB1555 packing, ground-scroll/damping/integration, inclusive footprint bounds, forward/reverse blend lifetime, state producer one-shot/repeat/max-burst gating, state-entry +0xF4-only reset, and inline producer RNG consumption. Collision/destruction regressions additionally verify their exact particle requests. The canonical probe source-validates hit/state particle fields and currently reports 3 hit-particle units (0 circular flag) and 7 state-particle states (4 repeat).


### Score-bar, gameplay-frame, and final-display contracts

`score_bar_runtime_test` freezes the label-verified Game[gafl]/Rects[inre] score-bar geometry, six dirty classes, 2/3 shield and 2/4 power convergence, exact 0..100 power clamp, preview invalidation, lives display, visibility transition, and upstream score/life threshold behavior. `score_bar_pixel_runtime_test` adds the canonical 16-bit TGA path, TESM glyph dispatch/metrics, exact `%0.7i` / `%i` formatting, xRGB1555 colorisation, final-life red style, hidden-text fade, meter COST masks, and weapon/life sprite rasterization. When `Interface.pak` is beside `Game.pak`, the canonical probe requires 91 TESM frames and score-bar sample FNV64 `0xd2f48984985f54d8`.

`gameplay_frame_runtime_test` freezes the outer executable order recovered from `0x5A18 -> 0x7070 -> 0x31AE0` followed by `0x5AB0 -> 0x30570 -> 0x30BC0`: score-bar pixels are produced before the world frame, the completed 416x480 game replaces only source X=0..415, score-bar X=416..575 survives, and mode-1 presentation emits the exact 32+416+160+32 layout. `presentation_runtime_test` additionally freezes `LegacyPresentationCommit::ImmediateQuickDrawWindowCopyNoSwap`: the original PEF uses a DrawSprocket front-buffer query for bounds discovery and a matching CWindow QuickDraw port as the copy destination, with no imported DrawSprocket back-buffer/swap API.


`level_select_effect_runtime_test` freezes the `0x2FC90/0x2FCC0/0x2FE40` front-end pulse contract: label-verified Game[gafl] 44..47 tuning, `lsca` green and `lscf` red xRGB1555 styles at blend 16, mode 0/1/2 trigger semantics, 24-tick acceptance and 16-tick failure teardown, center-preserving scaled-rectangle geometry, and immediate shared `COST` raster submission. The synthetic Debug suite is now **53/53 PASS**.


### Modern host-presentation regression rules

`modern_presentation_runtime_test` freezes the first platform-facing seam after the canonical renderer: exact xRGB1555-to-RGBA8888 channel expansion, bit-15 ignore behavior, 1,228,800-byte canonical upload size, aspect-fit/integer-fit/stretch viewport geometry, small-window integer fallback, clear-color letterboxing, nearest integer-ratio CPU scaling witnesses, backend error propagation, and rejection of an already host-sized legacy surface. The canonical probe additionally requires `modern bridge: RGBA8888 rowBytes=2560 1080pAspectFit=240,0 1440x1080`. The full synthetic Debug suite is **53/53 PASS**.


### Apple Metal adapter regression boundary

`apple_metal_public_header_test` verifies that the Apple presenter remains a
`ModernPresentationBackend` while its public header stays plain C++ and the
class remains non-copyable. On non-Apple platforms CMake does not enable
Objective-C++ or create `deimos_metal_backend`, so this test also guards the
portable public API boundary without linking Apple frameworks.

The Objective-C++ implementation itself is intentionally gated to Apple SDK
builds. Native validation must compile `deimos_metal_backend`, attach it to a
real `CAMetalLayer`, and compare nearest-mode screenshots against
`rasterize_modern_presentation_reference()`. The canonical gameplay/render
PAK probes remain unchanged because Metal is strictly downstream of the
640x480 xRGB1555 fidelity boundary.


### External original-data frame integration

`original_game_frame_preview_test` is a redistributable API/error-path guard and does not require original assets. `deimos_original_frame_probe /path/to/Paks` is now a strict real-data oracle gate: canonical Level-1 / Player-1 must produce initial FNV64 `0x9e8a7ec73b79b254`, tick-1 `0x44dede08075273f2`, and tick-30 `0x51d4a7eec9b0beef`, with `FPS_MaxRate=30`. The tick path advances the recovered terrain source rectangle and score-bar meter cache before executing the same gameplay-frame renderer. The Apple integration app consumes this persistent session at 30 Hz. Original files remain outside Git; iPadOS local bundle staging via `DEIMOS_ORIGINAL_PAK_DIR` affects only the generated local app bundle. The synthetic repository suite remains **53/53 PASS**.


### Preview player-control integration

`preview_player_control_test` freezes source-label-verified movement tuning, Game[gafl] slot-183 drift rejection, 7.8 max speed / 1.6 velocity convergence, release deceleration, opposing-direction cancellation, visible-area clamping, and inactive-player gating. This is intentionally an integration contract rather than a claim that the original InputSprocket/film-bit dispatcher is solved. With external canonical PAKs, `deimos_original_frame_probe` additionally requires the one-right-input tick full-frame FNV64 `0x6fd5c94a64dcb0c8` at player `(209.6,330)` / velocity `(1.6,0)`, while preserving all no-input frame oracles. The repository suite is **53/53 PASS**.


### Live world / weapon integration

`live_player_weapon_runtime_test` freezes semantic weapon selection and launch timing independently of unresolved host-film bits: Level-1 default Ion Cannon selection, the exact three serialized Ion requests (`icb ` -5/0, `icbf` 0/-8, `icb ` +4/0), Player-1 ownership, non-auto-repeat rising-edge behavior, delay gating, level-availability switching, and ground-weapon request production. `live_entity_screen_motion_test` freezes the corpus-corroborated visible-position sign contract (heading-0 +Y velocity moves north by subtracting Y velocity; heading-180 -Y moves south) plus terrain-scroll displacement.

With external canonical PAKs, `deimos_original_frame_probe` now exercises the terrain-row placement scheduler rather than booting the full Level corpus: it requires 2 initial placements/members, first later activation at tick 36, and 3 activated placements by tick 120. The same soak freezes removal orchestration at 24 allocated members, max 20 active, 0 entity/entity collisions and 0 surfaced collision-spawn requests for this input sequence, 7 finalized removals, 10 removal consequences, and 1 consequence spawn. The probe additionally requires canonical Level-1 Media Mask `cat1` to decode as 96x720 with an exact derived 5x5 world-cell scale over the 480x3600 level rectangle. It checks live-world integration frame witnesses `0x1eb1e07d4b6d038d`, `0x1e24b6143cd762ec`, and `0x13c37d4b847666f9`; all remain unchanged with real water sampling enabled. These are explicitly clean-integration regression witnesses rather than claims of original executable screenshot capture. `level_activation_runtime_test` freezes one-shot/source-order row release; `unit_rule_world_runtime_test` freezes sentinel-ID, Unit-ID/range/count/player/global rule facts; `terrain_runtime_test` freezes decoded-mask geometry/value-31 sampling; `collision_runtime_test` freezes lossless aggregate pair/spawn-fact propagation. The synthetic repository suite is **53/53 PASS**.


### Playable-host and live-HUD regression

`apple_objcxx_layout_test.py` now additionally rejects reintroduction of the local-event-monitor input path and requires the fail-fast live bootstrap, direct `DeimosGameWindow` responder, Space/Z air-fire binding, and accepted-fire diagnostic. `score_bar_pixel_runtime_test` proves absent/`none` locked weapon descriptors restore cleanly without requiring a sprite. `collision_runtime_test` preserves the source player owner on both pair-damage legs, which the world layer consumes for score attribution.

External canonical live witnesses after the level-availability HUD fix are initial `0x1eb1e07d4b6d038d`, first air-fire `0x1e24b6143cd762ec`, tick-120 `0x13c37d4b847666f9`. The unchanged original-data/static witnesses remain the stronger guard that no presentation or terrain regression was hidden by this UI correction.

### Playable WIP 3 runtime gate (2026-08-30)

`deimos_playable_runtime_probe /path/to/Paks` is the external-data gameplay gate for the native-host bugs reported after the first actually playable build. It uses canonical Level 1 and asserts the integrated player crash lifecycle (dying tick 185, respawn tick 266, lives 3 -> 2, shield reset, player-effect construction and non-zero particle execution), accepts canonical Plasma Bomb secondary fire, then performs a 3000-tick primary-fire/periodic-ground-fire soak. The soak requires resident live history to remain bounded (`maxResident <= 128`, `finalResident <= 32`), more than 1000 finalized records to have been physically pruned, and at least one shipped-`0x12CA0` 128px lifetime cull.

`deimos_original_frame_probe` retains the historical static frame oracles unchanged. Its live integration witness now expects 16 resident members and 10 resident groups at tick 120 after finalized-history compaction; 9 removals / 12 consequences include two exact `0x12CA0` far-offscreen deletions; one player-hit effect is produced; max active particles is 25 and max active live members is 19. Live frame hashes are `0x1eb1e07d4b6d038d`, `0xa0fc41ac06687be2`, `0x055b51228f651199`. The first-fire hash change is intentional because playable live mode no longer drives the unrecovered weapon-power HUD channel toward a fabricated 100% target.


### Playable WIP 6 secondary/respawn/reticle gate (2026-08-30)

`deimos_playable_runtime_probe /path/to/Paks` now requires the canonical Plasma Bomb crosshair to be enabled as `pbta`, remain exactly at Player 1 plus `(0,-121)`, expose normal frame 0 and locked frame 1 during the opening ground-target approach, accept the ground launch, and reduce the left `bsde` shields exactly 4.0 -> 3.6. Crash/respawn remains dying@185 / respawn@266 / lives=2, charge activates after 15 ticks and releases through `icps`, and the 3000-tick stress remains bounded.

The native Apple wrapper regression requires `flagsChanged:` handling for Shift ground fire. Destruction/runtime tests freeze the shipped `0x27E50 -> 0x34B90` player-owner cleanup and render/state tests freeze entry at `stateSpriteFrameMin`. Manual framebuffer dumps at ticks 266, 270, 276, 282, 300, 311, 312 and 320 verify no GET READY text remains after respawn.

Static original-data hashes remain `0x9e8a7ec73b79b254`, `0x44dede08075273f2`, `0x51d4a7eec9b0beef`, `0x6fd5c94a64dcb0c8`. Restoring the shipped live `pbta` reticle intentionally changes only live witnesses to initial `0xbdf7558de9357ff7`, first air-fire `0x036bb03279ae5b48`, and tick-120 `0x8e4063956c4df5cc`. Synthetic suite remains **53/53 PASS**.

### WIP7 menu/front-end gate

`apple_objcxx_layout_test.py` now also freezes the discoverable front-end contract: launch menu, pause menu, Controls surface, original Option/Command/Space Player-1 mappings, modern aliases, and Escape pause handling. Core synthetic coverage remains 53/53 PASS. With canonical PAKs, the original-data frame and playable-runtime probes remain unchanged from WIP6, including Plasma Bomb ground hit and `pbta` normal/locked reticle behavior.

### WIP8 animation / AI-ordering gate (2026-08-30)

WIP8 retains the 53-test synthetic suite and adds focused assertions inside the existing behavior/runtime tests for the recovered animation field compiler, 24-direction heading mapping (105 -> 7, 255 -> 17), strict animation delay, finite stop plus same-tick `Animation Has Stopped` rule transition, visual-only RotateToTarget, and the delayed-member `2 -> 1` skip / `1 -> 0` same-tick activation contract.

The canonical static/external frame hashes remain unchanged at `0x9e8a7ec73b79b254`, `0x44dede08075273f2`, `0x51d4a7eec9b0beef`, and `0x6fd5c94a64dcb0c8`. WIP8 intentionally re-freezes only the live-world witnesses: live initial `0xcd72678207b195b7`, first air-fire `0x800f06651d29406a`, and tick-120 `0x267609db3ba6dbcc`, with 15 residents / 9 groups and peak 18 active in that probe.

`deimos_playable_runtime_probe` now freezes the investigated full-WIP8 opening-lane lifecycle at dying@171 / respawn@252. It still requires Plasma Bomb damage to the left `bsde` exactly 4.0 -> 3.6, `pbta` normal/locked reticle behavior at `(0,-121)`, 15-tick Ion Cannon charge activation plus `icps` release, and a bounded 3,000-tick stress run. The oracle change is justified by the isolation experiment documented in `WIP8_ANIMATION_AI_ORDERING.md`; it was not accepted merely because the number moved.

Final rerun after documentation freeze: `cmake --build` PASS, **53/53** CTest PASS, `deimos_reference_probe` PASS, `deimos_original_frame_probe` PASS, and `deimos_playable_runtime_probe` PASS. The final stress witness is maxResident=96, finalResident=27, maxActive=96, pruned=1871, farCulled=213; crash/respawn remains 171/252.
