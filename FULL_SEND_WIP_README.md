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

## Regression checkpoint

- 53/53 synthetic tests PASS
- `deimos_playable_runtime_probe`: PASS
  - crash: dying@185, respawn@266, lives=2, playerEffects=7, maxParticles=90
  - Plasma Bomb secondary fire accepted
  - stress3000: maxResident=91, finalResident=14, pruned=1762, farCulled=201
- canonical `Game.pak` definition probe: PASS

Original Ambrosia assets/PAKs are intentionally excluded.

## WIP 4 addition — live charge + HUD power

The selected air weapon's original hold-to-charge fields are now active. For Level 1, hold **Z/Space** instead of tapping it: after 15 game ticks the Ion Cannon charge effect activates and the HUD power meter begins filling. Releasing fires the charged `icps` stream. Ordinary tap fire still works.

Current dedicated probe also reports:

```text
charge: activation=15 ticks maxObserved=20% release=icps
stress3000: maxResident=111 finalResident=18 pruned=2264 farCulled=269
```

The exact binary caller that determines charged release count remains an isolated fidelity item; all Units/timers/IDs used here come directly from the original Weapon/Unit Definitions.
