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

The repository suite is currently **34/34 PASS**. The optional canonical `Game.pak` probe additionally validates all compiled collision/destruction/terrain-media/player-runtime fields and reports 436 collision-enabled states, 135 air / 251 ground Unit Definitions, 8 pickup Unit Definitions (4 coin / 1 mult / 1 exli / 2 shie), 2 Player Definitions, player globals 100/10/1, death-money IDs `calg/cals/casg/cass`, 67 shadow casters, 4 ground-obstacle colliders, 12 any-media death spawners, 3 non-`none` media-impact units, and fixed water IDs `spti/spsm/spme/spla`, **0** stock
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
