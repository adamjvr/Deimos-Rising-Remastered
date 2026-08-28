# Roadmap

## Phase 0 — Evidence intake and provenance

**Substantially complete; intentionally remains open for additional archives.**

Exit/maintenance criteria:

- hash/catalog every supplied archive;
- preserve Mac data/resource forks;
- identify build/version relationships;
- separate official material, reference material, and community modifications;
- keep proprietary evidence local/untracked while committing reproducible manifests and findings.

## Phase 1 — Binary, resource, and serialization reconstruction

**Current active phase; resource/data half is now well advanced.**

Completed/confirmed:

- Mac 1.0.6 PEF/container and import baseline;
- four canonical PAKs recovered and read directly by clean code;
- Local-over-PAK provider architecture reconstructed;
- legacy tagged-text transform/grammar decoded across 473 resources;
- all level placement records parsed and validated;
- v10005 film header/input-stream boundaries substantially mapped.

Remaining Phase 1 exit criteria:

- map PEF relocations, TOC/import glue, entry points, and major gameplay functions;
- expand Windows installer and establish Mac↔Windows code/data correspondences;
- produce typed definitions for unit, weapon, player, ID/float/color/text/string/rect tables;
- finish replay layout/action-bit mapping including two-player semantics;
- document defaults, bounds, and cross-resource references with confidence levels;
- keep original art/audio/data as the canonical asset tier.

## Phase 2 — Deterministic gameplay reconstruction

Exit criteria:

- implement entity/state-machine interpreter from recovered unit definitions;
- reconstruct level sequencing, terrain/media sampling, spawning, movement, weapons, projectiles, collision, damage, scoring, power-ups, camera/scrolling, two-player behavior, menus/preferences, timing, and audio triggers;
- feed v10005 input films into the clean simulation and use them as deterministic regression oracles;
- add controlled behavior probes where recordings are insufficient.

## Phase 3 — Portable clean core completion

Exit criteria:

- deterministic simulation independent of platform/UI APIs;
- original PAK + loose Local provider fully integrated;
- restored/upscaled resources can override by the same exact resource identities;
- all canonical game definitions and levels load through typed clean parsers;
- automated tests cover resource lookup, serialization, replay input, timing, and gameplay invariants.

## Phase 4 — Native playable remaster

Exit criteria:

- macOS and iPadOS playable end-to-end using the clean core;
- original assets serve as initial canonical presentation;
- keyboard/controller/touch, rendering, audio, preferences, menus, campaign, two-player behavior, and replay support operational;
- fidelity mode matches original behavior before optional modernization.

## Phase 5 — Cross-platform completion

Exit criteria:

- Linux and Windows targets operational;
- deterministic cross-platform tests agree;
- packaging/controller/audio/rendering integration complete.

## Phase 6 — Restoration, upscale, and fidelity hardening

Exit criteria:

- graphics restored/upscaled systematically while retaining exact resource identity and original fallbacks;
- audio restoration remains nondestructive/comparison-tested;
- collision/timing/rendering discrepancies closed against original evidence;
- optional modernization remains separable from canonical behavior.
