# Roadmap

## Phase 0 — Evidence intake and provenance

**Substantially complete; intentionally remains open for additional archives.**

Maintain hashes, forks, provenance, build relationships, and separation of official/community evidence.

## Phase 1 — Binary, resource, serialization, and behavior-contract reconstruction

**Current active phase; transition kernel, spawn path, and normal entity-construction path are now substantially recovered.**

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
- group/appearance RNG, constructor gates, serial identities, parent safe references, float RNG, initial position/speed/heading, state-zero entry, and cumulative group delay recovered.

Remaining Phase 1 exit criteria:

- recover the rare special single-member parent-container path and remaining world/list-registration semantics around `0x33220`;
- bind major movement/tracking/rotation/hit/damage/collision routines to decoded fields;
- expand Windows installer and establish Mac↔Windows code/data correspondences;
- finish replay action-bit mapping including second-player semantics;
- document remaining behavioral defaults/bounds with confidence labels.

## Phase 2 — Deterministic gameplay reconstruction

**Started at the transition-kernel level; not yet a full entity simulation.**

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

Exit criteria:

- extend the now-live headless normal constructor into world registration and the full per-member update interpreter;
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
