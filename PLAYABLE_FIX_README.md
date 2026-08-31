# Deimos Rising playable-host correction — 2026-08-30

This cumulative source snapshot includes the current Level-1 live-world work plus the macOS device-test correction for:

- silent Preview/smoke fallback (now fail-fast);
- macOS weapon input (direct key-window responder);
- Level-1 weapon HUD availability;
- locked/absent HUD weapon slots;
- collision kill-score attribution into live Player 1 score/extra-life state.

## macOS / EVE build and run

From the repository root after extracting this ZIP over the branch:

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

Run the bundle executable directly from Terminal for this test; do not use `open` for the first verification because the diagnostic lines should stay visible.

Expected startup includes:

```text
Deimos playable original-data host: ... liveObjects=2
Deimos selected air weapon: Air - Ion Cannon [aiic]
```

Press Space or Z once. Expected Terminal output includes:

```text
Deimos AIR FIRE accepted at tick ...: +5 members
```

## Controls

- Arrows or WASD — move
- Space or Z — selected air weapon
- X — selected ground weapon
- Tab or C — cycle level-available air weapons

## Current WIP8 regression

- synthetic suite: 53/53 PASS
- canonical Game.pak clean-core validation: PASS
- original-data full-frame/live probe: PASS
- live initial members: 2
- first air-fire construction remains live and deterministic
- static preview hashes remain unchanged
- WIP8 live hashes after animation/orientation + ordering closure:
  - initial: `0xcd72678207b195b7`
  - first air-fire tick: `0x800f06651d29406a`
  - tick 120: `0x267609db3ba6dbcc`
- tick 120: 15 resident members / 9 groups, max active 18
- playable lifecycle: dying@171 / respawn@252
- Plasma Bomb: `bsde` 4.0 -> 3.6 and `pbta` normal/locked reticle PASS
- Ion Cannon: 15-tick charge activation and `icps` release PASS
- stress3000 bounded at maxResident=96 / finalResident=27 / pruned=1871 / farCulled=213

Original PAK data is intentionally not included in this archive.
