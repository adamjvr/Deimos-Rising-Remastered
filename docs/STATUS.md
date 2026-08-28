# Status

## 2026-08-28 — Phase 1 state-runtime semantics milestone

### Evidence corpus

- `DR-EVID-001` — older StuffIt-packaged disc image; retained as a cross-version oracle.
- `DR-EVID-002` — Mac 1.0.6 installation fully recovered from StuffIt/HFS layers.
- `DR-EVID-003` — Windows PE32/NSIS distribution identified; payload expansion/correlation remains active.
- `DR-EVID-004` — add-ons/update/reference/mod/music corpus; Apple Bundle update and Perfect Demos evidence recovered.

Phase 0 remains open only for additional evidence. Active engineering is Phase 1, with the first proven Phase-2 runtime primitives now implemented behind tests.

### Clean runtime/data core

Implemented and tested:

- exact FourCC/resource names and IA/IC plate parsing;
- dependency-free stored-ZIP PAK reader with CRC32 validation;
- `Data/Local` override layer over canonical PAK content;
- recovered seven-bit legacy tagged-text transform and grammar;
- typed scalar/FourCC/RECT/RGB fields;
- canonical level, data-table, Text Format, and v10005 film loaders;
- typed unit, weapon, and player definitions;
- explicit unit states, nested spawn sets, five-slot state rules, and weapon-spawn records;
- complete 17-condition executable rule vocabulary;
- binary-confirmed first-true-rule evaluation semantics;
- exact/case-sensitive state-action lookup with unresolved labels preserved as runtime no-ops;
- timer/state-entry-counter/range transition primitives recovered from PPC;
- cross-definition unit-reference validation;
- PEF pattern-data/import/relocation probe with synthetic relocation tests.

### Real-corpus validation

The current clean core has been run against original 1.0.6 `Game.pak`:

- 763 files CRC-validated;
- 12 levels / 565 placed objects;
- 4 canonical replay films;
- 6 ID lists, 1 float table, 1 color list, 1 rect list, 5 string lists, 54 text formats;
- **386 units / 1,167 states / 532 spawn sets / 5,835 rules**;
- **5 weapons / 15 weapon spawns / 2 players**;
- zero unknown canonical rule-condition strings;
- zero invalid proven unit references;
- **44 active unresolved/no-op state-action occurrences** preserved exactly;
- 30 inert unresolved range-action occurrences.

The 44 active no-ops include 15 case-only `Wait for Player Approach` mismatches that exact PPC `strcmp` does not resolve.

Across all four canonical PAKs, 871 original files CRC-validate.

### Entity runtime findings now binary-confirmed

- State/action resolver: code `0x146F0`.
- Range handler: `0x15280`.
- Rule evaluator: `0x15550`.
- Animation update: `0x15930`.
- Inclusive integer RNG helper: `0x46580`.
- 15-bit LCG RNG: `0x553E0`.
- Exact string comparator: `0x57820`.
- Rule evaluator supports 17 conditions and five ordered slots.
- First true rule ends rule evaluation even when its action is a no-op.
- Range-rule threshold zero makes both within/not-within predicates false.
- Timer delay is inclusive `[min,max]` and fires on exact target-tick equality.
- Entity owns 20 persistent state-entry counters; counter checks occur immediately on state entry.
- Range transitions use exact-zero disable plus strict `<` comparison.
- Main per-entity ordering establishes timer -> animation -> rules -> later range handling.

### PEF/binary milestone retained

- 89,661 packed section-1 bytes expand exactly to 104,632 initialized bytes;
- one 860-block relocation program executes to 5,153 fixups;
- all 445 imports are consumed coherently;
- main transition vector resolves to code `0x4D540`, TOC/r2 section-1 offset `0x8000`;
- executable internally identifies itself as `1.0.6`, `Jan  2 2004`, `11:55:01`.

### Tests

Synthetic repository tests pass **15/15**. Original assets/binaries are used only by optional local reference probes and remain outside Git.

### Active reverse-engineering fronts

1. Recover spawn-set scheduling, volleys, delays, repeat behavior, and exact RNG-consumption order.
2. Bind movement/tracking/rotation fields to entity runtime functions.
3. Recover hit/damage/destruction and collision/terrain behavior.
4. Finish v10005 replay bit assignment/two-player semantics and turn films into deterministic simulation oracles.
5. Expand/correlate the Windows executable payload.
