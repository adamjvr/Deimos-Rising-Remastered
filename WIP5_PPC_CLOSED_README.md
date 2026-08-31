# Deimos Rising — Full Send WIP5 PPC-Closed Runtime

Base pushed milestone:
`c94f0ab5e4b2f339f28d730a1394cb702847d917`
(`re/phase1-runtime-recovery`)

This cumulative WIP closes three previously provisional host behaviors against the recovered shipped Deimos Rising 1.0.6 PEF using PPC Lab:

1. exact main-tick 128px post-movement lifetime/cull predicate and ordering (`0x12CA0`);
2. exact collision-spawn request position/owner/parent construction (`0x14F10`, `0x1516C..0x1525C`);
3. exact charge release semantics in `0x3B3C0`, including removal of the fabricated `OverloadTime` auto-release;
4. shipped `0x12BC0`/`0x12C10` collision + coin-pickup white glow pulse, plus consumption of player-impact `CollisionDamageResult` consequences.

The original PEF is NOT included. Target identity and behavior witnesses are recorded in `reverse/notes/PPC_LAB_106_RUNTIME_WITNESSES.md`.

Validation in the packaging workspace:

- 53/53 CTest PASS
- canonical Game.pak clean-core validator PASS
- original-data frame probe PASS
- playable runtime probe PASS
- tick-120 live max-active: 19
- 3000-tick stress: maxResident=114 finalResident=10 pruned=1862 farCulled=236

## Apply over pushed checkpoint on macOS

```bash
cd ~/GitHub/Deimos-Rising-Remastered || exit 1

git switch re/phase1-runtime-recovery
git fetch origin
git reset --hard c94f0ab5e4b2f339f28d730a1394cb702847d917

unzip -o ~/Downloads/Deimos-Rising-Full-Send-WIP5-PPC-Closed-2026-08-30.zip -d .

rm -rf build
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON \
  -DDEIMOS_BUILD_APPLE_HOST=ON \
  -DDEIMOS_BUILD_APPLE_SMOKE_APP=ON \
  -DDEIMOS_ORIGINAL_PAK_DIR="$PWD/reference/DR-EVID-002/canonical/Paks"

cmake --build build -j"$(sysctl -n hw.ncpu)"
ctest --test-dir build --output-on-failure

./build/deimos_playable_runtime_probe "$PWD/reference/DR-EVID-002/canonical/Paks"
./build/deimos_original_frame_probe "$PWD/reference/DR-EVID-002/canonical/Paks"

./build/deimos_apple_host_smoke.app/Contents/MacOS/deimos_apple_host_smoke
```

Do not commit this WIP until device playtesting is satisfactory.
