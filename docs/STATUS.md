# Status

## 2026-08-28 — terrain/media-runtime milestone

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

Synthetic repository tests pass **23/23**. Original assets/binaries are used only by optional local reference probes and remain outside Git.

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


### Player-target/motion findings now binary-confirmed

- The player target subsystem owns exactly two slots; player status `4` is active.
- Closest-player replacement uses strict `<`, preserving first-slot ties, and returns the player's signed index byte.
- Live `+0x108/+0x10C` = target velocity, `+0x110/+0x114` = velocity delta, `+0x118/+0x11C/+0x120` = target player/index/position.
- Live `+0xCC` selects PPC `0x16CC0`, the recovered Flee motion path.
- `0x15280` order is target refresh/no-player lifecycle/Hunt -> range -> Hold/Cyclic/Flee/convergence.
- Hunt `0x16FE0` uses two original LCG draws to build a random velocity envelope and remains RNG-active with no players unless a lifecycle action removes the entity.
- Hold `0x17C40` uses the deliberately negated normalized target vector; Cyclic `0x17B70` and Flee `0x16CC0` use their decoded speed/delta pairs.
- Canonical counts: 30 Hunt states, 7 Hold states, 40 Cyclic states, 9 Delete-on-no-player states, 9 Destruct-on-no-player states.
- The Mine constructor now uses the recovered two-slot player query instead of a synthetic fixed target.
- Shared-RNG first player-aware tick regression: 546 ticked -> 544 active; exactly two zero-delay timer removals (`grob` Destroy, `tptf` Delete); no removal from player/motion, rules, or range.

### Collision/damage findings now binary-confirmed

- Entity scan: `0x36CF0`; integer AABB helper: `0x12AD0`; shield/damage routine: `0x14F10`.
- Collision domain is derived from `isGroundBased_BOOL`: `grnd` for ground units and `air ` for all others.
- Candidate filtering requires opposite `harmlessToPlayers` classes, matching collision domain, active/collision-participating state, non-positive group delay, and the executable's asymmetric projectile flags.
- Integer bounds use independent PPC `fctiwz` conversion; touching AABB edges continue to radial test `0x42F80`.
- `0x42F80` is now fully recovered: it compares `sqrt(trunc(dx^2+dy^2))` against the sum of vertical-span-derived radii with strict `<`. Startup `0x429C0..0x42A00` fills the small-distance table with `sqrt(i)` for `i=0..16383`; there is no sprite-mask stage here.
- Pair damage is symmetric. `passHitsToOwner` redirects through a validated parent safe-reference; the second leg preserves the observed 1.0.6 quirk that tests candidate's flag but loads **self.parent**.
- `Entity_HitDelay=1.0` is a strict `current > last+delay` gate. Shields clamp at zero; collision-invulnerable states restore the old shield value after absorbed damage is calculated.
- On-hit state changes use a separate strict delay gate. The damage routine deliberately retains its pre-hit compiled-state pointer for same-call glow/collision-spawn fields after a state transition.
- Collision-spawn timing is `current >= last+delay`; non-repeat collision spawns are tracked per entity.
- Canonical collision corpus: 436 collision-enabled states, 55 pass-hits-to-owner states, 139 collision-invulnerable states, 151 player-collision states, 141 no-glow states, and 5 collision-spawn states.
- Player collision geometry is recovered from `0x12A00` + `0x33968..0x341C8`: player radius is `0.5 * truncated Rect height`, while the entity radius remains integer span/2. The shared `0x42F80` quantization means a raw 6.5-distance equality can still hit because `6.5^2` truncates to 42 before sqrt.
- Player-impact control flow `0x34090..0x34314` now has a bounded clean scanner: state/harmless/viewport gates, two pre-snapshotted active player slots, AABB + radial geometry, `passHitsToOwner`, canonical 100-point `Player_ImpactDamageToEntities`, reciprocal UnitDef `damage_FLOAT`, and player-status recheck are represented. Player inventory/stat mutation (`0x37580`) and full player damage/life semantics (`0x27100`) remain explicit callbacks.
- `UnitDef +0x4D4/+0x4DC` are now bound to `pickup_Type_ID` / `pickup_Value_INT`. Non-`none` pickups use the exclusive pickup branch: a failed pickup does no damage; a successful pickup consumes/destroys the entity and skips reciprocal impact. Canonical corpus: 8 pickup units (4 `coin`, 2 `shie`, 1 `exli`, 1 `mult`).
- Shield construction now follows `0x35E50..0x35EB0`: base + positive increment * (`gameContext+0x14 - 1`), clamped to max only on the positive-increment branch. The higher-level semantic name of game-context `+0x14` remains intentionally unresolved.
- Canonical Unit Definitions: 135 air / 251 ground collision domains, 226 harmless units, 9 player projectiles, 110 player-projectile-hittable units, 120 nonzero collision-damage units, 131 nonzero base-shield units, and 8 pickup units.
- Lethal ordinary collision and successful pickup can now execute the recovered `0x16300` destruction effects immediately when supplied a removal context, preserving same-call random-bonus RNG order. Later group cleanup is idempotent with respect to those already-processed effects.

### Destruction/group findings now binary-confirmed

- Ordinary destruction/effect routine `0x16300`, group/member removal `0x36120`, child-destruction helper `0x363C0`, child-deletion helper `0x364F0`, and the outer inactive-member cleanup around `0x36610` are represented in the clean core.
- `destructSpawn_ID`, `destructParticle_ID`, packed destruction color, notice, complete sound descriptor, coin count/ID, group-kill coin ID, child flags, obstacle, terrain-draw, random-bonus, deletion-spawn, and owner/child state flags are compiled directly from source-format names.
- UnitDef `+0x4B2/+0x4B3/+0x4B4` are proven as `destructCreateObstacle_BOOL`, `destructDrawToTerrain_BOOL`, and `destructReleaseRandomBonus_BOOL`.
- Group `+0xA4/+0xA8/+0xAC` are original member count, active member count, and destroyed-member count. Group kill is destroyed-count equality, not active-count exhaustion.
- Ordinary/group-kill coin rewards require player-attributed destruction and are suppressed for a member already marked as a consumed pickup. The special group FourCC `SERM` is exempt from ordinary group-removal semantics.
- Child propagation uses serial-only parent matching plus per-state opt-in flags, reproducing the original destroy-vs-delete distinction. Owner destruction on child removal uses the validated parent safe reference in the outer cleanup pass.
- Random bonus binding is exact for `Game[gafl]` 209..219 and `Objects[gaob]` 25..34, with source labels verified before use. Canonical thresholds are 70,78,82,84,87,91,95,98,100; ground-accuracy threshold 10; minimum progression 3; object IDs `rb01`..`rb10`.
- Canonical destruction corpus: 99 destruction-spawn units, 99 particle units, 77 destruction sounds, 28 ordinary coin-reward units, 15 group-kill reward units, 54 destroy-children units, 58 delete-children units, 13 obstacle creators, 32 terrain-draw units, and 7 random-bonus units.
- Empty clean-core `FourCC{}` and the serialized sentinels `none`/`NULL` are all treated as absent resource IDs, preventing synthetic fixtures from emitting phantom destruction resources.

### Terrain/media findings now binary-confirmed

- Ground-sensitive destruction/deletion helper `0x16880` is fully recovered as a media-routing function, not a generic predicate. Non-ground units and `doDeathSpawnOnAnyMedia_BOOL` bypass the media lookup; water suppresses the caller's original spawn and can emit a replacement water impact.
- UnitDef `+0x11E/+0x125/+0x128/+0x12B/+0x2E4` map to `castsShadows_BOOL`, `isGroundBased_BOOL`, `collidesWithGroundObstacles_BOOL`, `doDeathSpawnOnAnyMedia_BOOL`, and `mediaImpactSize_ID`.
- `0xFEE0` samples the 16-bit Media Mask and recognizes value `31` on the water-impact path. Sample coordinates are `trunc(x)+32`, `trunc(y)+worldYOrigin`.
- `Objects[gaob]` 6..9 are label-verified water-impact IDs `spti/spsm/spme/spla`; `tiny/smal/med /larg/smra/mera/lara` routing and RNG order are reproduced exactly.
- Ground-obstacle store `0x2A6D0/0x2A770/0x2A830/0x2A950` is reconstructed as an append-only persistent Rect list with vertical scroll shifting, inclusive edge overlap, and reset. `destructDrawToTerrain_BOOL` appends to this same store.
- `destructCreateObstacle_BOOL` is distinct: outer cleanup copies `castsShadows_BOOL` into live render state and calls `0x12F20`; the clean trace retains the obstacle rect/shadow fact while exact renderer/pixel mutation remains bounded.
- Canonical terrain/media corpus: 67 shadow casters, 4 ground-obstacle colliders, 12 any-media death spawners, 3 non-`none` media-impact units; fixed water IDs are `spti/spsm/spme/spla`.

### Active reverse-engineering fronts

1. Integrate the proven ground-obstacle overlap into the complete member tick, including the original position rollback/latch, then recover renderer/bitmap effects behind `0x12F20` and any terrain-image mutation beyond the persistent Rect store.
2. Reconstruct concrete player-side pickup/inventory semantics in `0x37580` and player shield/life damage in `0x27100`, then bind those mutations behind the already-recovered player-impact scanner.
3. Recover the special live `+0xCD` destruction path through `0x17E70`, and wire every non-collision destruction entry site through the same clean teardown orchestration.
4. Recover the rare special single-member parent-container / intrusive-list semantics around `0x33220` and bind an actual decoded Media Mask provider to the terrain/media runtime.
5. Expand Windows evidence and replay/action mapping after the remaining Mac gameplay-core boundaries are stable.
