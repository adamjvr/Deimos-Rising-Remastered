# Roadmap

## Phase 0 — Evidence intake and provenance

**Substantially complete; intentionally remains open for additional archives.**

Maintain hashes, forks, provenance, build relationships, and separation of official/community evidence.

## Phase 1 — Binary, resource, serialization, and behavior-contract reconstruction

**Current active phase; transition/construction/gameplay cores plus the sprite software-render backend are now substantially recovered.**

Completed/confirmed:

- Mac 1.0.6 HFS/application/PAK corpus recovered;
- Local-over-PAK content architecture reconstructed;
- 473 legacy tagged-text resources decode reproducibly;
- level/table/Text Format resource families typed;
- 386 units, 5 weapons, 2 players structurally typed and cross-reference validated;
- PEF packed data, import table, relocation stream, main transition vector, and TOC/r2 base decoded;
- exact state/action string resolution recovered;
- complete 17-condition rule dispatch reconstructed;
- five-slot first-true rule ordering reconstructed;
- timer RNG/trigger semantics reconstructed;
- 20-slot state-entry counter semantics reconstructed;
- range-transition threshold/comparison semantics reconstructed;
- relevant per-tick transition ordering established;
- spawn scheduler/volley/repeat behavior and asymmetric RNG-consumption order reconstructed;
- spawn target terrain-effect gate and Unit Definition memory anchors reconstructed;
- relative/absolute/rotated spawn geometry and legacy trig-table contract reconstructed;
- portable proven spawn-request seed implemented;
- normal `0x33220 -> 0x35BF0 -> 0x35CD0` group/member constructor path implemented headlessly;
- group/appearance RNG, constructor gates, serial identities, parent safe references, float RNG, initial position/speed/heading, state-zero entry, and cumulative group delay recovered;
- clean world registry implements active-member identity, safe handle+serial validation, duplicate/owned-type queries, and parent-first/player-fallback owner resolution;
- Lock/Link/Orbit owner-location state initialization and per-tick behavior recovered and placed in the original post-range/pre-spawn update slot;
- 1,024-entry atan-table generation and integer heading helper reconstructed.
- two-slot active-player table and closest-player query reconstructed;
- recurring Hunt/Hold/Cyclic/Flee motion primitives and no-player lifecycle behavior reconstructed;
- player-aware tick ordering integrated before owner-location/spawn phases;
- first-tick shared-world regression classified to two zero-delay timer removals;
- entity collision scan `0x36CF0`, integer AABB helper `0x12AD0`, and shield/damage routine `0x14F10` reconstructed;
- collision domains/source fields, opposite harmless-class policy, asymmetric projectile gates, symmetric pair damage, owner redirection, and the observed second-leg self-parent quirk bound to clean data;
- strict hit/on-hit timing, shield clamp/invulnerability, pre-hit-state effect ordering, collision-spawn timing, and ordinary destruction facts implemented headlessly;
- canonical collision-field corpus validated without changing shared constructor/first-tick RNG regressions;
- `0x42F80` radial collision recovered exactly as `sqrt(trunc(dx^2+dy^2)) < radiusSum`; the former precise/mask callback has been removed;
- player Rect/radius geometry, viewport gate, two-player impact loop, pickup exclusivity, owner-redirection, reciprocal damage ordering, and status recheck reconstructed in a bounded clean scanner;
- pickup type/value compiled fields bound to `pickup_Type_ID` / `pickup_Value_INT` and validated across canonical Game.pak (8 pickup units);
- level-scaled shield constructor math recovered from `0x35E50..0x35EB0` without changing RNG consumption;
- ordinary destruction effects `0x16300` and group/member teardown `0x36120` reconstructed, including child destroy/delete cascades, coin/group-kill rewards, random bonuses, deletion spawns, owner propagation, obstacle/terrain requests, and the `SERM` exemption;
- canonical random-bonus positional resources (`Game[gafl]` 209..219 / `Objects[gaob]` 25..34) bound with label verification;
- ground-sensitive removal helper `0x16880` recovered exactly, including Media Mask value-31 water routing, `mediaImpactSize_ID` replacement selection, and shared-RNG ordering;
- persistent ground-obstacle Rect store `0x2A6D0/0x2A770/0x2A830/0x2A950` implemented with inclusive overlap and vertical-scroll semantics;
- ground-obstacle hit consequence proven as zeroing live velocity `+0x10/+0x14` and setting stationary `+0x13C` (not position rollback), with clean stop/latch helper coverage;
- live `+0x19` proven as constructor-cached `air ` collision-domain bit, closing the obstacle-query gate;
- state `+0x356` / live `+0xCD` proven as `stateUseThisStateOnShieldDepletion_BOOL` cache; zero shields route to first marked state through `0x17E70` instead of ordinary destruction;
- concrete player pickup dispatcher/stat mutations, `0x27100` shield/damage, immediate `0x27E50` death entry, lifecycle switch `0x2A150`, and respawn initializer `0x29CC0` implemented, including strict timers, gameplay-start-gated life consumption, solo/multi entry coordinates, entry invulnerability, and game-over disable;
- shared 0x94-byte sprite visual base, state-entry visual targets, exact visibility/tint/scale ramps, scale-tolerance RNG, main/shadow layer mapping, `0x12F20` pass ordering, and ordered base/tint/glow/terrain render intents reconstructed as a headless renderer-request boundary;
- FORM/AIFC + QuickTime IMA4 resource decode reconstructed for the complete canonical `Audio.pak` / `Music.pak` corpus; DR-EVID-005 supplies an independent soundtrack identity oracle;
- paired sprite frame construction below `0x18D20` reconstructed through `0x1D780/0x1EEC0`: xRGB1555 color planes, transparent-key fallback, legacy 0..32 transparency weights, and row-1000 sentinels; canonical 2,460 surfaces hash to `0x9f9dcfba05b5089c`;
- exact `0x13460` shadow transform reconstructed, including label-verified `Game[gafl]` offsets -48/104/-6/8, air 0.5 scale, ground scale, `adjustShadowLocForScaling`, 0..32 shadow transparency, horizontal view offset, and terrain-submission coordinates;
- `0x18A40/0x19570` software backend reconstructed: 76-byte request contract, layer queue/flush groups, normal/overall/shadow/solid xRGB1555 compositors, clipping, scaled nearest-neighbor sampling, Sprite FX/Alpha toggles, and layer-0/1 terrain-target composition; canonical 14,760-pass render oracle hashes to `0x32290b39b091e970`;
- `0x12F20/0x12FA0/0x13460` semantic-to-raw request orchestration is now bound end-to-end, including exact world/HUD transforms, main-terrain +32 X basis, shadow-terrain -32 basis, effect-color packing, frame-cache resolution, immediate/queued routing, +0x90 terrain sequence stamping, and `0x100B0` horizontal-view stepping;
- `0xFA10/0xFA90/0xFBC0/0x10000/0x10120/0x10220` terrain surface/camera lifecycle reconstructed: label-verified 416x480x16 view, persistent full terrain raster, 545-row activation prime, exact vertical scroll accounting/end latch, and full 416x480 viewport copy (disproving the prior incremental strip-copy hypothesis);
- `0x44630/0x431F0/0x43340/0x438C0` particle lifecycle reconstructed end-to-end: 100+100 startup direction tables/cursors, eight preset families, color variation, ground-scroll tracking, damping/bounds/fade/prune semantics, and exact state/collision-hit/destruction producer binding at original RNG positions;
- `0x30D8C..0x30DCC` plus `0xBC60/0xBEB0` native QuickDraw copy geometry reconstructed: normal gameplay mode 1, label-verified 640x480x16 frame, 32+416+160+32 layout, exact game/score-bar CopyBits rectangles, and centered side-border behavior; `0xC470/0xC81C` prove DrawSprocket front-buffer use is bounds discovery only, `0xAE20/0xA640` create the destination CWindow, and the PEF has no GetBackBuffer/SwapBuffers import;
- `0x30F40..0x32A70` score-bar producer/cache cluster reconstructed: Game[gafl] 111..143 and Rects[inre] 0..15 layout contracts, six dirty classes, player/weapon score-bar resources, exact shield +2/-3 and power +2/-4 convergence with proven 0..100 power clamp, `clamp(lives-1,0,9)` life display, and upstream `0x29A10` score/extra-life threshold semantics;
- `0x31AE0/0x31D70/0x31EA0/0x32050/0x32250/0x32500/0x327B0` score-bar pixel stage reconstructed with canonical 160x480 `scor` TGA, sibling `Interface.pak` TESM/tesm 91-frame font, exact score/life formatting, cyan/red styles, dirty background restoration, meter COST masks, and weapon-preview raster; sample score-bar FNV64 `0xd2f48984985f54d8`;
- gameplay-loop ordering around `0x5A18` and `0x5AB0` closes normal visible-frame orchestration: score-bar dirty draw precedes `0x30570 -> 0x30BC0`, then the completed 416x480 game plus persistent 160x480 score bar feed mode-1 presentation; clean `render_legacy_gameplay_frame()` binds this order under regression;
- native host integration now reaches a visually validated macOS `NSView -> CAMetalLayer -> Metal` drawable; the external-data `OriginalGameFramePreview` loads user-owned `Game.pak` + `Interface.pak`, freezes the initial full frame at `0x9e8a7ec73b79b254`, and now advances recovered terrain/HUD state at canonical 30 Hz with tick-1/tick-30 frame oracles `0x44dede08075273f2` / `0x51d4a7eec9b0beef`;

Remaining Phase 1 exit criteria:

- recover the rare special single-member parent-container path and remaining original intrusive-list/pool semantics around `0x33220`;
- integrate remaining player lifecycle spawn/audio/UI side effects into the now-closed visible-frame orchestrator; score-bar semantic/pixel rendering, world composition, 576x480 source composition, QuickDraw geometry, no-swap display commit, and the adjacent level-selection acceptance/failure `COST` pulse are closed; the backend-neutral modern presentation seam is operational, the Apple Metal adapter has compiled successfully for macOS and iPadOS, and `deimos_apple_host` now owns the reusable NSView/UIView + CAMetalLayer integration; next graphics work is macOS keyboard-control validation, iPad live-session validation, instruction-closing the original input/replay dispatcher, controller/touch binding, Vulkan on Linux, and any remaining non-gameplay/front-end visual producers;
- finish remaining Flee trigger/lifecycle edges and bind them to decoded fields;
- expand Windows installer and establish Mac↔Windows code/data correspondences;
- finish replay action-bit mapping including second-player semantics;
- document remaining behavioral defaults/bounds with confidence labels.

## Phase 2 — Deterministic gameplay reconstruction

**Started as a deterministic headless world; transition, construction, owner/motion, collision/damage, player-impact geometry, and bounded destruction/group teardown are operational, but this is not yet a full game simulation.**

Already implemented from binary-confirmed behavior:

- exact action resolution/no-op behavior;
- pure 17-condition rule predicate layer;
- first-match rule evaluator;
- inclusive timer-delay mapping;
- state-entry tick bookkeeping;
- persistent state-entry counters;
- strict range-transition predicate;
- exact spawn scheduler and target eligibility;
- spawn position/heading construction and request-seed generation.
- bounded entity-vs-entity collision candidate scan with exact radial geometry;
- symmetric collision damage, shield depletion/invulnerability, delayed on-hit action, collision-spawn facts, and ordinary destruction state;
- bounded entity-vs-player collision scan with exact AABB/radial geometry, viewport gates, pickup branch, owner redirection, and reciprocal player-damage boundary;
- concrete player pickup/money/life/multiplier/shield mutation, damage/death-entry runtime, and status-1..4 life/respawn/game-over lifecycle;
- level-scaled shield initialization;
- ordinary destruction effects and two-stage group/member teardown, including child/owner propagation, reward facts, random-bonus selection, deletion spawns, obstacle/terrain requests, and `SERM` behavior.
- deterministic visual-state runtime and headless render intents for scale/visibility/tint, sprite face/frame, main/shadow layer selection, colorise/glow ordering, and terrain-submission distinction.
- exact indexed GIF sprite-plate decode, alpha-atlas frame extraction, loaded sprite-group cache semantics, lazy/high-frame dimension lookup, and `0x12940` scaled geometry refresh.
- portable AIFC/Apple IMA4 decode for the complete canonical Audio.pak + Music.pak corpus, with PCM-level music oracles and DR-EVID-005 soundtrack correlation.

Exit criteria:

- bind recovered player lifecycle spawn/audio/UI facts into full world orchestration and finish remaining destruction orchestration; validate `deimos_apple_host` in real macOS/iPadOS view hierarchies with CPU-oracle screenshot parity and map the same completed modern presentation packet to Vulkan/D3D swapchains without changing the canonical xRGB1555 frame;
- integrate world/entity construction, then reconstruct movement, weapons, projectiles, collision, damage, scoring, power-ups, camera/scrolling, two-player behavior, menus/preferences, timing, and audio triggers;
- feed v10005 recordings into the clean simulation as deterministic regression oracles;
- retain original assets as the canonical content tier.

## Phase 3 — Portable clean core completion

Exit criteria:

- deterministic platform-independent simulation;
- complete original PAK + Local provider integration;
- original/restored/upscaled resource tiers selectable through identical FourCC identities;
- comprehensive automated gameplay and serialization tests.

## Phase 4 — Native playable remaster

Exit criteria:

- macOS and iPadOS playable end-to-end;
- original assets provide initial canonical presentation;
- keyboard/controller/touch, rendering, audio, preferences, menus, campaign, co-op, and replay support operational;
- canonical-fidelity behavior is the default reference mode.

## Phase 5 — Cross-platform completion

Exit criteria:

- Linux and Windows operational;
- deterministic parity tests agree across platforms;
- packaging/controller/audio/rendering integration complete.

## Phase 6 — Restoration, upscale, and fidelity hardening

Exit criteria:

- graphics restored/upscaled nondestructively with original fallbacks;
- audio restoration comparison-tested;
- remaining collision/timing/rendering discrepancies closed against evidence;
- optional modernization remains separable from canonical behavior.
