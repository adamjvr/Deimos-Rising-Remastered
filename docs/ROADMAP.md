# Roadmap

## Phase 0 — Evidence intake and provenance

Exit criteria:

- every supplied archive hashed and cataloged;
- provenance and exact relationships between copies documented;
- all original payloads extracted losslessly into a local reference workspace;
- dual-fork Mac files preserved;
- primary versions/builds identified;
- original binaries/assets remain outside Git.

## Phase 1 — Binary/resource reconstruction

Exit criteria:

- executable architecture/container identified and mapped;
- PEF/PowerPC code, imports, exports, resources, strings, and major subsystems inventoried;
- Windows build, if supplied, cross-correlated against Macintosh build;
- resource and data-file formats documented;
- initial function/object correspondence ledger established.

## Phase 2 — Game-data and behavior reconstruction

Exit criteria:

- levels, terrain, entities, weapons, power-ups, enemies, projectiles, scoring, collision, timing, camera/scrolling, menus, save/preferences, two-player behavior, and audio behavior documented;
- confidence levels recorded for every inferred structure;
- synthetic parsers/tests cover recovered formats.

## Phase 3 — Portable clean core

Exit criteria:

- deterministic simulation/gameplay core separated from platform/UI layers;
- reconstructed data loaders consume independently described formats;
- no original executable code copied into implementation;
- regression harness compares captured reference behavior to clean implementation.

## Phase 4 — Native playable remaster

Exit criteria:

- macOS and iPadOS playable;
- keyboard/controller input, rendering, audio, timing, save state, menus, and core campaign operational;
- original assets loaded only from a user-owned local reference installation or replacement asset pack.

## Phase 5 — Cross-platform completion

Exit criteria:

- Linux and Windows targets operational;
- platform parity tests pass;
- packaging/build documentation complete.

## Phase 6 — Fidelity hardening and modernization

Exit criteria:

- timing/collision/gameplay discrepancies reduced through evidence-led validation;
- scalable rendering, modern displays, modern controllers, and quality-of-life improvements implemented without changing canonical gameplay by default;
- documented compatibility modes where behavior intentionally differs.
