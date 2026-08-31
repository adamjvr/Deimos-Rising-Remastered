# Deimos Rising Full-Send WIP6 — Secondary / Respawn / Reticle

Base Git checkpoint: `c94f0ab5e4b2f339f28d730a1394cb702847d917` on `re/phase1-runtime-recovery`.

This source snapshot contains clean-room/runtime code only. It does **not** contain the original Deimos Rising executable, PEF data/resource forks, StuffIt installer, HFS image, or original PAKs.

## WIP6 closures

- native macOS Shift secondary input via AppKit `flagsChanged:`; X remains secondary/ground fire;
- canonical Plasma Bomb ground-damage regression (`bsde` 4.0 -> 3.6);
- state visuals enter at serialized `stateSpriteFrameMin`;
- PPC-backed player-death owner cleanup (`0x27E50 -> 0x34B90`);
- canonical persistent Plasma Bomb reticle `pbta`, offset `(0,-121)`, `defa` layer, frame 0 normal / frame 1 locked;
- framebuffer verification that stale GET READY is absent after respawn;
- prior WIP5 PPC-closed 128px lifetime, collision-spawn ownership/position, charge behavior, hit/pickup glow, particles, crash/respawn and bounded-world fixes retained.

Original 1.0.6 PEF research identity (not included): SHA-256 `8e436c3babc582f1407ae6fed47e9749f1c930335ce4c794947e40b06b85eb29`.
