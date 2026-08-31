# Deimos Rising Full-Send WIP8 — animation/orientation + AI ordering freeze

Frozen: 2026-08-30.

This cumulative source snapshot continues WIP7 and closes the shared animation/orientation and live-member ordering faults identified in the WIP8 PPC/data audit. Original Ambrosia PAK/assets are not included.

## What changed

- recovered state animation fields compiled into live behavior;
- direction initialization from heading and visual-only RotateToTarget;
- strict animation cadence, loop/reverse/randomization support, finite stop, and same-tick Animation-Stopped rules;
- delayed member `1 -> 0` same-tick activation;
- rule/target/motion before screen movement/lifetime;
- owner modes before child spawn scheduling;
- ground-obstacle stop after due child spawn requests;
- following-frame vertical-scroll pause latch;
- Level-1 ground-placement audit retained binary-confirmed level-vs-child coordinate semantics rather than applying a global visual offset.

See `docs/WIP8_ANIMATION_AI_ORDERING.md` for the evidence and isolation work.

## Frozen validation

- CMake rebuild: PASS
- CTest: 53/53 PASS
- canonical Game.pak clean-core probe: PASS
- original-data frame probe: PASS
  - static hashes unchanged: `0x9e8a7ec73b79b254`, `0x44dede08075273f2`, `0x51d4a7eec9b0beef`, `0x6fd5c94a64dcb0c8`
  - live hashes: `0xcd72678207b195b7`, `0x800f06651d29406a`, `0x267609db3ba6dbcc`
  - tick 120: 15 resident members / 9 groups; max active 18
- playable-runtime probe: PASS
  - dying@171 / respawn@252 / lives=2
  - Plasma Bomb damages `bsde` 4.0 -> 3.6; `pbta` normal/locked PASS
  - Ion Cannon activates charge at 15 ticks and releases `icps`
  - stress3000: maxResident=96, finalResident=27, maxActive=96, pruned=1871, farCulled=213

## Safe apply/build on the active branch

This snapshot is intended to overlay the existing Deimos Rising repository tree; it does not contain `.git` or original PAK data. After extraction, configure/build/test normally, then run the three optional external-data probes against `reference/DR-EVID-002/canonical/Paks` when that local evidence workspace is present.

## Next engineering pass

Do not retune enemy positions or timing by eye. Continue with evidence-driven closure of remaining Hunt/target/flee/convergence and enemy-fire cadence semantics, then native audio/notice consumers and level-completion/game-over orchestration.
