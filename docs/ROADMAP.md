# Roadmap

## Phase 0 — Evidence intake and provenance

**Substantially complete; intentionally remains open for additional archives.**

Maintain hashes, forks, provenance, build relationships, and separation of official/community evidence.

## Phase 1 — Binary, resource, serialization, and behavior-contract reconstruction

**Current active phase; transition kernel, spawn path, normal entity construction, world/owner-location, player-target/motion, and the bounded entity collision/damage layer are now substantially recovered.**

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
- concrete player pickup dispatcher/stat mutations and `0x27100` shield/damage plus immediate `0x27E50` death entry implemented, including fixed Game/Objects positional contracts and money-drop decomposition;

Remaining Phase 1 exit criteria:

- recover the rare special single-member parent-container path and remaining original intrusive-list/pool semantics around `0x33220`;
- continue the later player life/respawn/game-over state machine, integrate the still-bounded live `+0x19` ground-obstacle tick gate plus renderer/terrain mutation beyond the recovered Rect store, and recover the special `0x17E70` destruction path;
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
- concrete player pickup/money/life/multiplier/shield mutation and immediate damage/death-entry runtime;
- level-scaled shield initialization;
- ordinary destruction effects and two-stage group/member teardown, including child/owner propagation, reward facts, random-bonus selection, deletion spawns, obstacle/terrain requests, and `SERM` behavior.

Exit criteria:

- finish the downstream player life decrement/respawn/game-over state machine, integrate the ground-obstacle stop around its remaining live `+0x19` gate plus renderer/terrain mutation beyond the recovered Rect store, the special `0x17E70` destruction branch, and remaining destruction entry-site orchestration;
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
