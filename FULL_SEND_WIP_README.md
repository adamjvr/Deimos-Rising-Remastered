# Deimos Rising Full-Send Playable WIP — 2026-08-30

This cumulative source snapshot includes the current live Level-1 playable-host work through:

- scheduled level placement activation (no all-at-once world spawn);
- live primary and ground/secondary weapon construction;
- live enemy/player collision and damage;
- destruction/removal consequences and particles;
- player death/respawn/lives lifecycle;
- score/lives/shield HUD state and weapon previews;
- dead-history compaction plus conservative far-offscreen host culling for long-run performance;
- deterministic original-data probes, including a 3,000-tick stress gate.

## macOS build/run

Extract this ZIP over `re/phase1-runtime-recovery` at base `b20475f977e4c00c370d366892b3b9291a118b15`, then from the repo root:

```bash
rm -rf build
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON \
  -DDEIMOS_BUILD_APPLE_HOST=ON \
  -DDEIMOS_BUILD_APPLE_SMOKE_APP=ON \
  -DDEIMOS_ORIGINAL_PAK_DIR="$PWD/reference/DR-EVID-002/canonical/Paks"
cmake --build build --target deimos_apple_host_smoke -j"$(sysctl -n hw.ncpu)"
./build/deimos_apple_host_smoke.app/Contents/MacOS/deimos_apple_host_smoke
```

Controls in this snapshot:

- arrows/WASD: move
- Z/Space: air weapon
- X/Left Shift/Right Shift: ground/secondary weapon
- C/Tab: cycle available air weapons

## WIP8 regression checkpoint

Post-freeze validation on 2026-08-30:

- 53/53 synthetic tests PASS
- canonical `Game.pak` clean-core probe: PASS (763 files, 12 levels, 386 units)
- `deimos_original_frame_probe`: PASS
  - static hashes unchanged: `0x9e8a7ec73b79b254`, `0x44dede08075273f2`, `0x51d4a7eec9b0beef`, `0x6fd5c94a64dcb0c8`
  - WIP8 live hashes: `0xcd72678207b195b7`, `0x800f06651d29406a`, `0x267609db3ba6dbcc`
  - tick 120: 15 resident members / 9 groups, max active 18
- `deimos_playable_runtime_probe`: PASS
  - crash: dying@171, respawn@252, lives=2, playerEffects=8, maxParticles=93
  - Plasma Bomb actual damage: `bsde` 4.0 -> 3.6; `pbta` reticle normal/locked PASS
  - Ion Cannon: 15-tick charge activation, 20% max observed in probe, `icps` release PASS
  - stress3000: maxResident=96, finalResident=27, maxActive=96, pruned=1871, farCulled=213

The crash/live-hash shift is intentionally frozen only after the WIP8 animation/ordering isolation experiment documented in `docs/WIP8_ANIMATION_AI_ORDERING.md`.

Original Ambrosia assets/PAKs are intentionally excluded.

## WIP 4 addition — live charge + HUD power

The selected air weapon's original hold-to-charge fields are now active. For Level 1, hold **Z/Space** instead of tapping it: after 15 game ticks the Ion Cannon charge effect activates and the HUD power meter begins filling. Releasing fires the charged `icps` stream. Ordinary tap fire still works.

The dedicated WIP8 probe currently reports:

```text
charge: activation=15 ticks maxObserved=20% release=icps
stress3000: maxResident=96 finalResident=27 maxActive=96 pruned=1871 farCulled=213
```

The exact binary caller that determines charged release count remains an isolated fidelity item; all Units/timers/IDs used here come directly from the original Weapon/Unit Definitions.
