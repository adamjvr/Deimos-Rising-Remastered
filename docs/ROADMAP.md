# Roadmap

## Phase 0 — Evidence intake and provenance

**Current: substantially complete; remains open for additional archives.**

Exit criteria:

- hash/catalog every supplied archive;
- preserve Mac data/resource forks;
- identify build/version relationships;
- separate official material, reference material, and community modifications;
- maintain a local, untracked evidence workspace while committing reproducible manifests and findings.

## Phase 1 — Binary, resource, and serialization reconstruction

**Current active phase.**

Exit criteria:

- map PowerPC PEF sections, loader/imports, relocations, entry points, TOC usage, and major functions;
- expand the Windows build and establish Mac↔Windows correspondences;
- reconstruct the PAK/Local resource manager and exact four-character tag semantics;
- decode all game-data formats (`leve`, `unde`, `plde`, `wede`, `idli`, `flli`, `coli`, `tefo`, `stli`, `reli`, `film`);
- record field meanings, defaults, bounds, and cross-references with confidence levels;
- preserve original art/audio as the canonical asset tier for the remaster.

## Phase 2 — Deterministic gameplay reconstruction

Exit criteria:

- reconstruct world/level sequencing, terrain, spawning, entities, weapons, projectiles, collision, damage, scoring, power-ups, camera/scrolling, two-player behavior, menus, preferences, timing, and audio triggers;
- parse and replay captured `.film` recordings against the clean simulation;
- add behavior probes where recorded films are insufficient;
- create regression fixtures from independently described observations.

## Phase 3 — Portable clean core

Exit criteria:

- deterministic simulation/gameplay core independent of UI/platform APIs;
- resource provider supports original PAKs plus loose `Data/Local` overrides;
- exact original resource IDs remain stable while restored/upscaled assets can replace them;
- reconstructed parsers load all canonical game definitions and 12 levels;
- automated tests cover resource lookup, serialization, timing, and gameplay invariants.

## Phase 4 — Native playable remaster

Exit criteria:

- macOS and iPadOS playable end-to-end using the clean core;
- original assets serve as the initial canonical presentation layer;
- keyboard/controller/touch input, rendering, audio, save/preferences, menus, campaign, two-player behavior, and replay support operational;
- fidelity mode matches original behavior before optional modernization changes.

## Phase 5 — Cross-platform completion

Exit criteria:

- Linux and Windows targets operational;
- cross-platform deterministic tests agree;
- platform packaging and controller/audio/rendering integration complete.

## Phase 6 — Restoration, upscale, and fidelity hardening

Exit criteria:

- original graphics are restored/upscaled systematically while retaining tag identity and original fallbacks;
- audio restoration is nondestructive and comparison-tested;
- collision/timing/rendering discrepancies closed against original evidence;
- optional modernization features remain separable from canonical behavior.
