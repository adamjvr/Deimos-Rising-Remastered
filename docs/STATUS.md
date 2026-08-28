# Status

## 2026-08-28 — Phase 1 world/owner-runtime milestone

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

Synthetic repository tests pass **19/19**. Original assets/binaries are used only by optional local reference probes and remain outside Git.

### Spawn runtime findings now binary-confirmed

- Scheduler: `0x15B40`; state-entry initializer: `0x17CB0`.
- State-entry RNG order is rate -> volley -> inter-entity delay.
- Repeat re-arm RNG order is inter-entity delay -> volley -> next rate.
- The single canonical reversed rate `110..20` is preserved under PPC signed-remainder behavior.
- UnitDef `+0x12A` = `canBeSpawnedOnlyWhenPlayersActive_BOOL`.
- UnitDef `+0x12E` = `adjustInitialLocForOwnerScale_BOOL`.
- UnitDef `+0x132` = `terrainEffect_BOOL`.
- Terrain-effect targets require a non-stationary parent with terrain effects enabled.
- Spawn geometry reproduces absolute/relative offsets, owner scaling, heading adjustment, rotation, fused multiply-add/subtract ordering, and truncation toward zero.
- Startup builds 360-entry sin/cos float tables from exact constant `0x3C8EFA35`.
- The clean core now emits a portable proven subset of the original 44-byte spawn request.

### Entity-construction findings now binary-confirmed

- Top constructor `0x33220`, group loop `0x35BF0`, live-member constructor `0x35CD0`.
- Full 44-byte request semantic layout is mapped for Unit ID, x/y, world-Y adjustment, heading fields, player owner, stationary/terrain options, parent pointer+serial, and velocity multiplier.
- Group/appearance selection `0x369F0` executes **before** player/duplicate/cap gates and therefore can consume RNG for a subsequently rejected request.
- Normal group container size is 188 bytes with its own serial counter, separate from live-member serials.
- Safe parent references are pointer+serial pairs validated by `0x36AB0`; clean code uses portable handle+serial.
- Float RNG `0x465E0` is implemented, including equal-endpoint no-draw and original reversed-bound signed-span behavior.
- Initial location `0x37930` and canonical initial motion `0x37B50` are implemented in the clean normal constructor path.
- State entry/spawn-record initialization occurs before cumulative per-member group-delay RNG.
- All 386 canonical Unit Definitions validate through the recovered initial-member math.
- A shared-RNG real-corpus probe produced 386 normal groups / 546 live members; 5 requests carried delete-existing-owned-type intent.
- Canonical initial-motion coverage: 36 variable-speed units, 9 randomized-location units, one initial hunter (`Mine[mine]`), zero Burst/Implode units, one reversed X/Y offset range (`Screw Mk 2[sc02]`).

### World/owner-location findings now binary-confirmed

- Safe reference validator `0x36AB0` = pointer/handle + live serial `+0x9C` + active lifecycle.
- Duplicate Unit-ID scan `0x36AF0` and owned Unit-ID/player scan `0x36BE0` are implemented as clean world queries.
- State `+0x32E/+0x32F/+0x330` map exactly to Lock/Link/Orbit owner-location flags.
- Owner initializer `0x33600` runs after construction and ordinary state changes.
- Owner resolution prefers a valid parent safe reference, then falls back to signed player owner `+0xD8`.
- Lock `0x37130`, Link `0x37230`, and Orbit `0x37350` are implemented in their original post-range/pre-spawn tick slot.
- Orbit angular step is `int(trunc(live velocity X))`; this odd field reuse is independently supported by initial-motion stores at `+0x10/+0x14`.
- Startup atan-table generation and `0x43090` quadrant conversion are reconstructed.
- Canonical data uses 156 Lock states / 10 Link states / 8 Orbit states across 71 / 5 / 4 Unit Definitions.
- All 546 members from the shared-RNG corpus constructor register as active clean-world members.

### Active reverse-engineering fronts

1. Recover the rare special single-member parent-container path and remaining original intrusive-list/pool details around `0x33220`.
2. Recover target selection/tracking/hunting and bind the rest of movement/rotation fields.
3. Recover hit/damage/destruction and collision/terrain behavior, beginning with collision scan `0x36CF0`.
4. Finish v10005 replay bit assignment/two-player semantics and turn films into deterministic simulation oracles.
5. Expand/correlate the Windows executable payload.
