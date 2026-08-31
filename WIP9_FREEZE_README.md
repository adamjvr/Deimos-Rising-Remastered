# Deimos Rising Remastered — WIP9 Freeze

Date: 2026-08-30 (America/New_York)

## Scope

WIP9 closes canonical 1.0.6 target/flee/convergence edge semantics and the missing
live-visual-heading input to rotation-adjusted enemy/entity spawns. It does **not** retune
enemy behavior or fire rates by feel.

## Binary evidence

The original 1.0.6 application PEF was recovered again from the canonical StuffIt -> SMI
-> HFS chain. Local research witness:

- SMI: 81,788,928 bytes
- PEF data fork: 2,045,976 bytes
- PEF SHA-256: `8e436c3babc582f1407ae6fed47e9749f1c930335ce4c794947e40b06b85eb29`

The original binary is not distributed in this snapshot.

Recovered PPC closures:

- `0x146F0` state-entry flee latch
- `0x15280` target/no-player/Hunt/range dispatcher
- `0x161C0` current visual heading
- `0x16CC0` flee acceleration
- `0x17510` flee target mode dispatch
- `0x15B40` / `0x17CB0` enemy/entity spawn cadence re-audit

## Canonical corpus

- 386 Unit Definitions
- 1,167 states
- 17 explicit non-`none` flee-mode states
- 8 north-on-no-active-player Units
- 1 south-on-no-active-player Unit
- 532 spawn sets
- no canonical pause-rotation-while-spawning sets

## Behavioral result

- flee accelerates toward authored destinations;
- explicit flee target RNG occurs before state spawn-runtime RNG;
- no-player north/south flee uses the original target initializer and first-tick return;
- range entry into flee installs target now but accelerates beginning next tick;
- rotation-adjusted child/projectile spawns use live sprite-frame heading;
- enemy firing cadence remains the already recovered state spawn-set scheduler.

## Regression / isolation

WIP8 -> WIP9 four-way 3000-tick Level-1 differential:

| configuration | max resident | final | pruned | far culls |
|---|---:|---:|---:|---:|
| WIP8 flee + stale heading | 96 | 27 | 1871 | 213 |
| WIP8 flee + WIP9 heading | 85 | 26 | 1655 | 152 |
| WIP9 flee + stale heading | 84 | 18 | 2219 | 246 |
| full WIP9 | 84 | 15 | 1773 | 136 |

Restoring both WIP8 paths reproduces the WIP8 stress witness exactly.

Full-WIP9 live diagnostics: 23 explicit flee activations, 1,766 entity spawn-due events,
1,092 rotation-adjusted spawns and 48 visual-heading differences.

## Freeze gates

Post-documentation freeze validation completed successfully:

- **53/53 CTest PASS**
- canonical `deimos_reference_probe` PASS, including 17 explicit flee states and 8 north / 1 south no-player flee Units
- `deimos_original_frame_probe` PASS with every WIP8 static/live hash unchanged
- `deimos_playable_runtime_probe` PASS with dying@171 / respawn@252, Plasma Bomb 4.0 -> 3.6, Ion Cannon release, and bounded WIP9 stress at 84 max resident / 15 final / 1773 pruned / 136 far-culls
- diagnostic counts reproduced: 23 flee activations / 1766 spawn-due / 1092 rotated spawns / 48 visual-heading differences

See `docs/WIP9_FLEE_TARGET_FIRE_HEADING.md`.
