# Status

## 2026-08-29 — Apple Metal host-view integration milestone

### Evidence corpus

- `DR-EVID-001` — older StuffIt-packaged disc image; retained as a cross-version oracle.
- `DR-EVID-002` — Mac 1.0.6 installation fully recovered from StuffIt/HFS layers.
- `DR-EVID-003` — Windows PE32/NSIS distribution identified; payload expansion/correlation remains active.
- `DR-EVID-004` — add-ons/update/reference/mod/music corpus; Apple Bundle update and Perfect Demos evidence recovered.
- `DR-EVID-005` — standalone ten-track soundtrack archive; Theme Song independently binds released music to canonical `mu03`.

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

Synthetic repository tests pass **53/53**. Original assets/binaries are used only by optional local reference probes and remain outside Git.

### Modern/native presentation status

- The deterministic host seam remains the exact canonical 640x480 xRGB1555 frame converted to immutable RGBA8888 after all recovered raster work completes.
- `deimos_metal_backend` is a separate Apple-only CMake target; `deimos_core` remains free of Objective-C++, Metal, QuartzCore, and native window dependencies.
- `AppleMetalPresentationBackend` uploads one RGBA8 texture, clears letterbox regions, applies the precomputed `ModernViewport`, selects nearest/linear sampling, draws one textured quad, and commits one `CAMetalDrawable`.
- The backend does not recalculate scaling and cannot mutate canonical renderer state. It has now compiled successfully with both macOS and iPadOS Apple toolchains.
- `deimos_apple_host` is the next integration layer: `AppleMetalHostView` creates/owns an `NSView` or `UIView` whose backing layer is `CAMetalLayer`, tracks point-size to physical-pixel drawable geometry, enforces main-thread AppKit/UIKit use, and calls `present_modern_frame()` for completed canonical 640x480 surfaces.
- The host public header remains ordinary C++; Linux builds/tests prove no Apple types or dependencies leak into `deimos_core`. Native compile/view-hierarchy/pixel-parity validation of the new host target is the current Apple gate.

### Software render-backend findings now binary-confirmed

- `0x18A40/0x19570` request submission and software compositor are reconstructed through a portable xRGB1555 surface API.
- The original request is 76 bytes; low flags are overall transparency `0x1`, shadow `0x2`, solid tint/glow `0x4`, and terrain target `0x8`.
- `0x1A450/0x1A650/0x18B20` layer queueing and flush groups are implemented; layers 0/1 are one-shot terrain layers and groups are `0..1`, `2..5`, `6..15`.
- Normal, overall-transparency, shadow, and solid-color compositors preserve color-key fallback, the 0..32 transparency plane, row-1000 sentinel, right/bottom-exclusive clipping, and exact xRGB1555 integer arithmetic.
- Scaled paths use PPC-truncated extents, untruncated-float centering, and nearest-neighbor integer-ratio sampling.
- Shadow partial coverage uses the recovered single-precision law `trunc(base + 0.032f * mask^2)`.
- Executable diagnostic strings identify the default-enabled `Sprite FX` and `Sprite Alpha Drawing` toggles; FX-off forces scale 1/effect 0, alpha-off falls back to the transparent color key.
- Main terrain requests use one-shot layer 1 and terrain shadows layer 0; per-request terrain pixel composition is therefore now covered by the same compositor.
- Canonical stress validation runs 14,760 software-render passes over all 2,460 stock frame surfaces and hashes to `0x32290b39b091e970`.

### Terrain surface/camera findings now binary-confirmed

- `0xFBC0` loads the level `im16` background into one persistent 16-bit terrain surface; the temporary decoded image is disposed after the copy.
- `Game[gafl]` indices 54/55/56 are now label-bound as `VisibleGameWidth=416`, `VisibleGameHeight=480`, `ReqDisplayDepth=16`.
- `0xFA90` initializes the source view to the bottom-most 416x480 crop with fixed source X `+32`, requested vertical delta `+1`, and vertical progress `481`.
- `0xFA10` performs 545 world-row activation calls from source bottom through source top-64; this is simulation activation, not terrain bitmap strip copying.
- `0x10220` changes only source-Rect/scroll state and publishes the applied vertical delta; `0x10000` adds the end latch and one-row `top-64` activation.
- Normal +1 scrolling preserves the executable's one-pixel end quirk: progress reaches the terrain height with source top still at `1`.
- `0x10120` copies the **entire 416x480 viewport** from the persistent terrain surface into the visible gameplay surface on each call, using horizontal source left `max(0, horizontalOffset+32)`.
- Direct `0x30BC0` ordering is now implemented as terrain queue group 0 -> full terrain viewport copy -> group 1 -> particle raster `0x43BA0` -> group 2. The caller's draw latch gates only the viewport copy and particle pass; all three queue flushes remain unconditional.

### Particle/world-frame findings now binary-confirmed

- `0x43BA0` is the particle subsystem's direct 16-bit visible-surface rasterizer, not native presentation or a generic post-process.
- The surrounding executable cluster is bounded as `0x43340` particle construction, `0x438C0` update/prune, `0x43BA0` raster, and `0x44550` clear/destruction.
- Canonical `Game[gafl]` indices 144..148 are label-verified as `Particle_Gravity=0.96`, `Particle_ColorVariationAdjust=0.12`, `Particle_FringeColorAdjust=0.6`, `Particle_BlendAmountRate_Short=3.0`, and `Particle_BlendAmountRate_Long=1.0`.
- The raster record uses active byte `+0`, core/fringe xRGB1555 colors `+2/+4`, blend amount `+8`, float X/Y `+0x0C/+0x10`, and velocity X/Y `+0x14/+0x18`; positive system delay `+0x468` skips the whole object.
- Screen X subtracts `0x100A0`; clipping is the exact float contract `x/y>=0` and `x/y+7<visibleExtent` before truncate-toward-zero conversion.
- The exact 7x7 radial kernel is reconstructed, including the five-pixel core-color plus, radial `q+22/q+10/q+6/q` transparency bands saturated at 31, and the legacy center discontinuity `q>6 ? q-7 : q`.
- `render_legacy_world_frame()` closes the recovered `0x30BC0` composition sequence while intentionally stopping before native display ownership.

### Audio/music resource findings now confirmed

- portable FORM/AIFC parser and Apple/QuickTime `ima4` decoder implemented;
- 34-byte/channel packets decode 64 samples with low-nibble-first ordering and QuickTime predictor-continuity semantics;
- decoder matches independent canonical Music.pak PCM decoding sample-for-sample;
- all 96 canonical Audio.pak effects decode as mono 44.1 kHz/16-bit IMA4 (3,133,376 PCM frames total);
- all 3 canonical Music.pak resources decode as stereo 44.1 kHz/16-bit IMA4 with fixed PCM checksums;
- canonical `mu03` under-declares its FORM size by 76 bytes; clean parser preserves QuickTime-compatible tolerance;
- DR-EVID-005 Theme Song ID3 title is `Music 3[mu03]`, and soundtrack cross-correlation independently binds `mu03`, `inmu`, and `ammu` material.

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

### Core-edge live flags now binary-confirmed

- Member constructor `0x35F88..0x35FA0` sets live `+0x19 = (UnitDef+0x08 == 'air ')`; the ground-obstacle path therefore excludes air-domain members before testing `collidesWithGroundObstacles_BOOL`.
- State parser `0x41698..0x416A8` maps `stateUseThisStateOnShieldDepletion_BOOL` to compiled state `+0x356`.
- Member constructor `0x35DAC..0x35DF0` caches the existence of any such state in live `+0xCD`.
- On zero shields, `0x14F10` awards score and either calls ordinary destruction (`+0xCD == 0`) or `0x17E70`, which enters the first marked state (`+0xCD != 0`).
- Stock canonical `Game.pak` has 0 marked shield-depletion states; the executable path is retained through synthetic compatibility regression.
- Core-edge checkpoint remains covered inside the current **41/41 PASS** suite; canonical constructor/first-tick seeds remain unchanged.


### Visual/render-request findings now binary-confirmed

- Entities and players share the 0x94-byte sprite base initialized by `0x12650`; face/frame, scaled dimensions/half extents, geometry dirty, tint/visibility/scale triplets, draw layer, terrain-draw, colorise, collision glow, and the cached sprite/frame handle are now offset-mapped.
- UnitDef visual defaults are compiled directly from `initialScalePercent_INT +0x1AC`, `initialScalePercentTolerance_INT +0x1B0`, `initialVisibilityPercent_INT +0x1B4`, `drawLayer_ID +0x2E0`, `castsShadows_BOOL +0x11E`, and `adjustShadowLocForScaling_BOOL +0x12C`.
- State visual fields are parser/runtime-correlated for sprite face/frame, parent direction, tint color, colorise, terrain draw, required scale/visibility/tint, and their deltas.
- `0x12750` reproduces visibility/tint convergence and `0x12840` reproduces scale convergence, including the original asymmetric zero clamp and geometry-dirty behavior. Initial scale tolerance consumes the shared legacy RNG in its original signed inclusive range.
- `0x12F20` is now bounded as the render wrapper: visibility gate -> optional shadow request -> main request. Live `+0x37/+0x38` are temporary pass selectors used by the world renderer, not persistent entity properties.
- `0x12FA0` main requests preserve base/tint/collision-glow ordering; `stateDoColorise_BOOL` suppresses only the normal base request. `stateDrawToTerrain_BOOL` bypasses the ordinary layer switch and remains a distinct terrain submission path.
- Main draw-layer mapping is recovered, including the corrected PPC FourCCs `plwe -> 9` and `play -> 10`; zero/`none` is normalized to `defa`. Shadow requests use a separate recovered layer domain: default/grou ground 2, grhi 4, default air/recognized air-player-HUD 6.
- `0x12940` now closes the geometry loop through the recovered sprite cache: current face/frame resolve through `0x19AD0`, `0x19C10/0x19CA0` apply PPC-truncated scaled dimensions with lazy loading/high-frame fallback, and signed half extents are rebuilt exactly. A `none` face zeros half extents while leaving stale width/height, matching the binary.
- Canonical visual corpus: 17 scale-tolerance units, 62 colorise states, 2 terrain-draw states, 111 nonzero-tint states, 584 non-100 visibility states, and 506 non-100 scale states. Raw draw-layer counts are `defa=156, grou=17, grhi=68, ailo=10, aihi=51, plwe=5, play=0, plsh=2, plef=0, plui=10, atmo=0, hud=17, none=50`.

### Sprite-resource/cache findings now binary-confirmed

- `0x19AD0` is the loaded sprite-group/frame lookup; frame indices at or above the loaded count fall back to frame 0, while `none` does not resolve.
- `0x19C10` returns stored frame dimensions at scale 1.0 and otherwise multiplies each axis by scale and truncates toward zero with PPC `fctiwz`. `0x19CA0` adds absent-group lazy loading and retry; `0x19EE0` exposes loaded frame count.
- `0x18D20` builds a 16-byte loaded-group record (marker, FourCC, frame count, frame-pointer list) and publishes it only after the complete frame set succeeds, so failed/partial loads are not observable. State entry stores the resolved frame pointer at live sprite-base `+0x50`.
- `0x1F140/0x1F1C0/0x1F340/0x1F4E0/0x1F540/0x1F5B0` form the exact alpha-plate atlas scanner. It consumes decoded GIF palette indices, uses the alpha plate's second byte as the separator marker, finds marker-bounded cells, then trims each cell by its own top-left palette value.
- The clean resource layer includes a dependency-free indexed GIF87a/89a decoder, exact atlas extraction, atomic cache publication, high-frame fallback, lazy dimensions, and the `0x12940` geometry integration. Negative frame indices are rejected as an explicit safety divergence from the original out-of-bounds legacy behavior.
- Canonical sprite corpus: 124 alpha plates, 124 color plates, 123 existing alpha/color pairs with equal dimensions, and 2,463 extracted alpha frames. `PDLI` is the stock alpha-only exception. Examples: `PL1B` 7 frames (frame 0 = 53x43), `EXLG` 12, `BOCR` 3, `GLOW` 12.

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
- Player-impact control flow `0x34090..0x34314` now has a bounded clean scanner: state/harmless/viewport gates, two pre-snapshotted active player slots, AABB + radial geometry, `passHitsToOwner`, canonical 100-point `Player_ImpactDamageToEntities`, reciprocal UnitDef `damage_FLOAT`, and player-status recheck are represented. Concrete pickup/stat mutation (`0x37580`) and player shield/damage/death-entry semantics (`0x27100`/`0x27E50`) are now implemented in `player_runtime.cpp`; collision keeps narrow callbacks only as a subsystem-orchestration boundary.
- `UnitDef +0x4D4/+0x4DC` are now bound to `pickup_Type_ID` / `pickup_Value_INT`. Non-`none` pickups use the exclusive pickup branch: a failed pickup does no damage; a successful pickup consumes/destroys the entity and skips reciprocal impact. Canonical corpus: 8 pickup units (4 `coin`, 2 `shie`, 1 `exli`, 1 `mult`).
- Shield construction now follows `0x35E50..0x35EB0`: base + positive increment * (`gameContext+0x14 - 1`), clamped to max only on the positive-increment branch. The higher-level semantic name of game-context `+0x14` remains intentionally unresolved.
- Canonical Unit Definitions: 135 air / 251 ground collision domains, 226 harmless units, 9 player projectiles, 110 player-projectile-hittable units, 120 nonzero collision-damage units, 131 nonzero base-shield units, and 8 pickup units.
- Lethal ordinary collision and successful pickup can now execute the recovered `0x16300` destruction effects immediately when supplied a removal context, preserving same-call random-bonus RNG order. Later group cleanup is idempotent with respect to those already-processed effects.

### Player pickup/shield/death-entry findings now binary-confirmed

- Player Definition `+0x48/+0x4C/+0x50/+0x54` are default shield, warning threshold, base hit percentage, and shield hit delay; `+0x60/+0x64/+0x70` are max lives, initial lives, and life spawn; `+0xBC/+0xC8/+0xCC/+0xD0` are death spawn, active hit spawn, shield warning object, and defence-bonus object.
- `0x37580` is now concrete: canonical pickups are 4 `coin`, 1 `mult`, 1 `exli`, 2 `shie`; executable-retained `air `/`grnd` branches reject while player invulnerability `+0xCE` is set.
- Money pickup adds only nonzero values; multiplier follows `1->2->3->4->5->10`; extra life is capped but still consumes at max; shield pickup adds and clamps to `[0,100]`.
- `0x27100` stores the player hit tick before invulnerability, scales incoming UnitDef damage directly by `shieldBaseHitPercentage`, does not clamp shield, and enters death only at shield `< 0`; zero shield survives. Hit glow still runs for an invulnerable accepted hit.
- `0x27E50` immediate death entry emits `death_Spawn_ID`, clears hit bookkeeping, decomposes money in 50/10/5/1 units, sets status 3/current tick, and raises invulnerability. It does **not** decrement lives; life consumption is now separately recovered in `0x2A150`.
- Fixed player contracts are now label-verified: `Game[gafl]` 161/162/167 = impact damage / hit-spawn delay / entity-hit delay and `Objects[gaob]` 2..5 = `calg/cals/casg/cass` money units. Canonical values are 100/10/1.
- Player lifecycle `0x2A150` is recovered: status 1 game-over, 2 waiting/entry, 3 dying, 4 active; all timer expirations use strict `currentTick > statusSince + duration` comparisons.
- PlayerDef compiled layout is not serialization order: `gameOver/dying/finalDying/entryInvulnerability` occupy `+0x80/+0x84/+0x88/+0x8C`, solo/multi entry coordinates `+0x90..+0x9C`, `entry_Spawn_ID +0xA0`, and `entry_InitialDelay +0xB8`.
- Status 3 uses `finalDyingTime` only when exactly one life remains, then the caller's gameplay-start latch controls life decrement; lives remaining call respawn initializer `0x29CC0`, while zero lives enter status 1 until `gameOverTime` later disables the player.
- `0x29CC0` selects solo/multi entry coordinates via live `+0xCD`, writes velocity from the executable's shared literal `{0,0}`, writes status 4/current tick, and emits `entry_Spawn_ID`; post-death respawn restores default shield and clears hit/warning clocks.
- Status-4 invulnerability expires only after the strict `entry_InvulnerabilityTime` deadline and only when both the external `0x5CF0` gate and live `+0xCF` permit it.

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
- `destructCreateObstacle_BOOL` is distinct: outer cleanup copies `castsShadows_BOOL` into live render state and calls `0x12F20`; the clean core now reconstructs the deterministic visual/render-request boundary, exact source frame surfaces, exact shadow transform, software clipping/blending, queue/backend submission, per-request terrain-target composition, persistent terrain surface lifetime, camera scrolling, full-viewport background copying, exact particle raster, and the outer `0x30BC0` world-composition order, and the downstream mode-0/mode-1 QuickDraw presentation-copy geometry. Score-bar production and final display ownership are now also recovered: DrawSprocket supplies fullscreen bounds, a matching CWindow owns the QuickDraw destination, and the executable has no DrawSprocket back-buffer/swap import.
- Canonical terrain/media corpus: 67 shadow casters, 4 ground-obstacle colliders, 12 any-media death spawners, 3 non-`none` media-impact units; fixed water IDs are `spti/spsm/spme/spla`.

### Sprite-frame/shadow findings now binary-confirmed

- `0x1D780` constructs each cropped 16-bit frame surface with xRGB1555 color pixels, a plate-wide transparent color key taken from the third color-plate pixel, and an optional second 16-bit transparency plane.
- `0x1EEC0` maps alpha red5 into the legacy inverted transparency domain `0=opaque`, `1..31=blend`, `32=transparent`; fully transparent rows use sentinel `1000` in their first word. When no secondary plane is needed the legacy blitter falls back to transparent-key comparison.
- Canonical 123 paired IA/IC plates produce 2,460 normal frame surfaces, 3,115,564 color words, 3,115,564 transparency words, 6,341 row sentinels, and aggregate surface FNV64 `0x9f9dcfba05b5089c`.
- `0x13460` shadow geometry is exact: canonical offsets are air `-48,104`, ground `-6,8`; air scale is `0.5*entityScale`, ground scale is `entityScale`; the scaling-adjust flag only alters the air offset basis.
- `0x100A0` is the bounded horizontal view offset used by world-space main/shadow transforms, and `0x100B0` now supplies its exact +/-1 step/clamp/direction-latch behavior. Main terrain stamps use `trunc(worldX)+32` plus `0xFEC0` world/background Y origin, while terrain shadows independently use the recovered `0x13460` shadow transform with its -32 terrain basis.
- `0x10C20` maps visibility into the original 0–32 shadow transparency domain; `0x13460` clamps the request to a minimum transparency of 20.

### Active reverse-engineering fronts

1. Validate the new macOS keyboard player-control bridge and the 30 Hz live session on iPadOS; then instruction-close the original InputSprocket/film-bit dispatcher, bind controller/touch to the same portable snapshot, and proceed to Vulkan on Linux.
2. Bind the recovered player life/respawn/game-over lifecycle spawn/audio/UI facts into full world orchestration and continue into the remaining active-player movement/weapon boundaries.
3. Wire every remaining non-collision destruction entry site through the same clean teardown orchestration.
4. Recover the rare special single-member parent-container / intrusive-list semantics around `0x33220` and instruction-close the exact `0xFEE0` world-to-Media-Mask address transform now that the decoded resource provider is live.
5. Expand Windows evidence and replay/action mapping after the remaining Mac gameplay-core boundaries are stable.


## 2026-08-28 — particle lifecycle and gameplay-producer milestone

Direct PPC reconstruction now closes `0x44630/0x431F0/0x43340/0x438C0` and binds all three recovered gameplay particle producers. Startup creates parallel 100-entry unit/varied direction tables and seeds two cursors after 302 RNG draws; the varied table uses 1/.85/.70/.55 factors. `0x43340` accepts `tiny/smal/med /larg` and `tici/smci/meci/laci` as 5/10/20/40-particle presets with 3/5 velocity magnitudes. `0x438C0` is ground-scroll-aware damping/integration plus inclusive -32/+32 X and visible-Y footprint bounds and long-rate fade/prune.

State particles are now proven to run before the timer/rule portion, with live +0xF0 last-burst tick and +0xF4 burst count. State entry resets only +0xF4, preserving the previous timestamp. Collision-hit and destruction producers construct the same 24-byte request and can execute inline through an optional particle context, preserving original shared-RNG placement without forcing global particle state into bounded headless tests.

The canonical probe source-validates all new fields and reports **3 hit-particle units (0 circular flag), 7 state-particle states (4 repeat), and 99 destruction-particle units**. All established hashes/seeds and 386/546/544 gameplay counts remain unchanged. The native-presentation follow-up raises the Debug synthetic suite to **38/38 PASS** and label-validates the 640x480x16 = 32+416+160+32 frame contract.


## 2026-08-28 — Native presentation geometry milestone

Direct PPC tracing beyond `0x30BC0` closes the original QuickDraw copy stage. The second `0x30BC0` argument remains the terrain/particle draw latch, while the third independently gates post-world presentation. Frame mode byte `+0x04` dispatches mode 0 to `0xBC60` and mode 1 to `0xBEB0`; the normal gameplay constructor call at `0x56AC` supplies mode 1.

`Game[gafl]` indices 52..60 are now label-verified as 640x480 minimum frame, 416x480 visible game, 16-bit depth, 160x480 score bar, and 32-pixel left/right borders. The identity is exact: **32 + 416 + 160 + 32 = 640**. `0xBEB0` performs two QuickDraw `CopyBits` operations from the source canvas: game `{0,0,480,416}` and score bar `{0,416,480,576}`, placing them into the centered 640x480 frame after the left border. Wider displays also `PaintRect` the two side strips black. Mode 0 instead copies one complete 640x480 frame.

`LegacyPresentationConfig`, plan generation, and a bounded portable xRGB1555 plan executor now freeze those semantics. `0x9E40` was also classified precisely as a `SetGWorld` activation helper, not a score-bar renderer. That upstream producer is recovered in `SCORE_BAR_RUNTIME.md`, and the element-specific original-pixel path is now closed in `SCORE_BAR_PIXEL_RUNTIME.md` using canonical `scor` + `Interface.pak` TESM assets. The synthetic suite is **41/41 Debug PASS**; canonical probe reports `native presentation frame: 640x480x16 = 32 + 416 + 160 + 32` with all prior hashes, counts, and RNG seeds unchanged. See `NATIVE_PRESENTATION_RUNTIME.md`.


### Score-bar producer/cache and score findings now binary-confirmed

- `0x30F40/0x31400/0x317E0/0x31AE0` own the normal gameplay score-bar producer/cache immediately upstream of the already-recovered `0xBEB0` 160x480 presentation copy.
- `Game[gafl]` 111..143 and `Rects[inre]` 0..15 are now label-verified as the complete static score-bar layout/rate contract; six dirty classes cover score, life symbol, life count, all three weapons, shield, and power.
- Shield converges +2/-3 and power +2/-4; upward convergence occurs only at player status 4. Relocated PEF TOC data proves the power clamp constants are exactly 0.0/100.0.
- Lives render as `clamp(lives - 1, 0, 9)`, reconciling canonical max semantic lives 10 with the maximum displayed count 9.
- Player Definition score-bar base/power/shield sprite resources are compiled, and all 5 canonical Weapon Definitions expose validated score-bar preview face/frame descriptors.
- `0x299F0/0x29A00/0x29A10` now provide concrete clean score production: ordinary awards multiply by the live bonus multiplier, extra-life comparison is strict `newScore > threshold`, only one threshold is consumed per call, and canonical thresholds are initial 10000 / additional 30000 with Game[182] adjustment 10000.
- The semantic score-bar regression is now joined by original-pixel and full gameplay-frame orchestration regressions; the current Debug suite is **41/41 PASS** while canonical renderer hashes and constructor/motion RNG seeds remain unchanged.


## 2026-08-29 — Score-bar producer + final legacy display-commit milestone

The 160x480 score-bar producer/cache is now modeled directly from `0x30F40..0x32A70`: Game[gafl] 111..143 and Rects[inre] 0..15 are label-verified, all six dirty classes are distinct, shield converges +2/-3, power +2/-4 with exact 0..100 clamp, lives display as `clamp(lives-1,0,9)`, PlayerDef/WeaponDef score-bar resources are preserved, and the upstream `0x29A10` score/extra-life threshold producer is implemented. Canonical score thresholds are 10000 initial / 30000 additional with Game[182] adjustment 10000; all five canonical weapons provide preview descriptors.

The final Mac display commit is also closed. `0xC470` calls the sole imported `DSpContext_GetFrontBuffer` only to obtain fullscreen bounds through `0x44B50`; `0xAE20` then creates a matching `NewCWindow`, and `0xBEB0`/`0xBC60` ultimately `CopyBits` into that QuickDraw window port. The PEF imports neither `DSpContext_GetBackBuffer` nor `DSpContext_SwapBuffers`, so there is no hidden post-copy flip to reconstruct. `LegacyPresentationCommit::ImmediateQuickDrawWindowCopyNoSwap` records this historical semantic while leaving modern backends free to use native swapchains.

The score-bar pixel + gameplay-frame follow-up raises the Debug synthetic suite to **41/41 PASS**. With canonical `Interface.pak` present, the probe additionally validates 91 TESM frames and score-bar sample FNV64 `0xd2f48984985f54d8`; both established renderer hashes, 386/546/544 entity counts, and both RNG seeds remain unchanged.


## 2026-08-29 — Original score-bar pixels + complete visible-frame orchestration

The score-bar pixel consumer is now closed through original assets. Canonical `Scorebar[scor].TGA` decodes as a 160x480 xRGB1555 surface. The small text renderer resolves `tesp` to the 91-frame `TESM/tesm` pair in sibling `Interface.pak`; scores use `%0.7i`, lives use `%i`, normal numeric text is cyan `#94DEE6`, and the final-life zero uses red `#FF0000`. Dirty redraw restores the static panel first, then uses the shared recovered compositor for glyphs, life symbols, weapon previews, and shield/power meter COST masks. Canonical sample score-bar FNV64 is `0xd2f48984985f54d8`.

The outer gameplay loop also resolves the missing orchestration order: `0x5A18 -> 0x7070 -> 0x31AE0` draws the score bar before `0x5AB0 -> 0x30570 -> 0x30BC0` performs world composition and mode-1 presentation. `render_legacy_gameplay_frame()` now binds score-bar pixels, the exact group0/terrain/group1/particles/group2 world pass, 576x480 source composition, and the recovered 640x480 presentation plan in one portable boundary. Current Debug suite: **41/41 PASS**.


## 2026-08-29 — Level-select acceptance/failure visual closure

The residual `0x2F7A0..0x2FE40` `COST` path is now classified and implemented as a front-end level-selection effect, not a gameplay HUD layer. `Formats[gate]` runtime ordinals 27/28 are label-verified as `lsca`/`lscf`; canonical acceptance loads green `0x03e0` at blend 16 and failure loads red `0x7c00` at blend 16. `Game[gafl]` 44..47 supply exact scale pulses 0.18→2.0 and 0.25→2.0. The updater fades blend one step toward 32 while scale runs 0→max→0, yielding 24-tick acceptance and 16-tick failure lifetimes before exact reset. The shared `COST` rectangle request is now represented by the clean renderer. Debug suite: **42/42 PASS**. Gameplay visible-frame orchestration remains unchanged and closed.


## 2026-08-29 — Modern host-presentation seam

The clean renderer now has its first post-canonical host boundary. `modern_presentation_runtime` requires the exact recovered 640x480 legacy display frame, expands xRGB1555 to tightly packed RGBA8888 only after deterministic raster completion, and calculates aspect-fit, integer-fit, or explicit stretch viewports in physical drawable pixels. A `ModernPresentationBackend` interface isolates future Metal/Vulkan/D3D code from simulation and legacy raster state.

A dependency-free nearest-neighbour reference presenter supplies byte-level letterbox/scaling parity without becoming the shipping renderer; it intentionally rejects linear filtering as a deterministic oracle because sampler details differ by graphics API. The reference probe now locks the canonical bridge at `rowBytes=2560` and 1920x1080 aspect-fit viewport `240,0 1440x1080`. Synthetic Debug suite: **53/53 PASS**.


## 2026-08-29 — Native Apple host visual pass + external original-data frame bridge

The corrected `deimos_apple_host` Objective-C++ scope now builds and the macOS smoke application has been visually validated through a real `NSView -> CAMetalLayer -> Metal` path. The captured diagnostic frame preserves the recovered 32+416+160+32 geometry, crisp nearest sampling, letterboxing, and Retina drawable mapping. The Apple backend itself had already compiled for both macOS and iPadOS.

`OriginalGameFramePreview` now supplies the next integration boundary without embedding copyrighted assets: given a user-owned directory containing `Game.pak` and `Interface.pak`, it loads Level 01, the full 480x3600 background, canonical score-bar panel/TESM font, Player-1 score-bar/player sprite family, and three weapon-preview groups, then executes `render_legacy_gameplay_frame()` to produce the canonical 640x480 xRGB1555 display. `deimos_original_frame_probe` reports a whole-frame FNV64 so the first canonical run can become a parity oracle. The Apple smoke app auto-detects this data, falling back to the synthetic diagnostic frame if unavailable. Optional `DEIMOS_ORIGINAL_PAK_DIR` CMake staging copies the two user-owned PAKs only into a local generated Apple smoke-app bundle for iPad/device tests. Repository synthetic suite: **53/53 PASS**.


## 2026-08-29 — original-data live Metal session milestone

The user-validated macOS Metal path now has a deterministic original-data live session rather than a single static frame. The first complete Level-1 / Player-1 frame is frozen at FNV64 `0x9e8a7ec73b79b254`; recovered terrain-scroll + score-bar ticks freeze tick 1 at `0x44dede08075273f2` and tick 30 at `0x51d4a7eec9b0beef`. Canonical `Game[gafl]` supplies `FPS_MaxRate=30`.

`OriginalGameFramePreview` now retains terrain camera state, score-bar cache, game/source/display surfaces, and tick/render sequence state. The Apple integration host uses a 30 Hz main-run-loop timer on macOS and a preferred-30-FPS `CADisplayLink` on iPadOS to execute one recovered tick, one canonical gameplay-frame render, and one Metal present. The fixture now includes a clearly bounded modern host-control bridge using canonical Player-1 speed/delta tuning, while the original InputSprocket/film-bit dispatcher remains intentionally unclaimed. It still excludes unproven full entity-owned render-queue record lifetime, weapon production, collision/spawn orchestration and audio; those are the next live-loop bindings rather than approximations.


## 2026-08-29 — First live player-control integration

The original-data Metal session now accepts a portable directional input snapshot. Canonical source data supplies Player-1 `active_DefaultMaxSpeed_FLOAT=7.8`, `active_VelocityDelta_FLOAT=1.6`, and label-verified Game[gafl] 183 `Player_TopGameAreaLimit=13`. The integration deliberately remains named `PreviewPlayerControl`: the original InputSprocket/film bit dispatcher is not yet instruction-closed, so the modern host mapping is not presented as recovered replay semantics.

The macOS smoke host tracks independent key-down/key-up for arrows and WASD. No-input frame hashes remain initial `0x9e8a7ec73b79b254`, tick 1 `0x44dede08075273f2`, tick 30 `0x51d4a7eec9b0beef`. A separate one-right-input oracle is Player `(209.6,330)`, velocity `(1.6,0)`, full frame `0x6fd5c94a64dcb0c8`. The synthetic Debug suite is now **53/53 PASS**.


## 2026-08-29 — live world scheduler / rule facts / removal integration

The native original-data session now follows the recovered terrain-row encounter schedule instead of constructing the full Level-1 placement corpus at boot. Canonical `le01` starts with only two activated placement groups/members; `sourceTop-64` reaches the third placement on tick 36. PlayerWorld slot 0 is authoritative for live Player 1. The recovered five-slot state-rule evaluator now receives Unit-ID/range/global facts from the live world, with the canonical 2,773 sentinel-`none` `Is Tracking Player` template slots correctly remaining inert.

Collision scans now receive one `LegacyRemovalContext` per tick, lethal `0x16300` consequences preserve same-call RNG order, and the outer `0x36610` inactive-member pass finalizes group accounting/child-owner propagation/deletion and reward spawns. Consequence spawns are constructed only after traversal; the persistent ground-obstacle Rect list scrolls with the camera and participates in the recovered ground-member stop/latch query. The live session now binds the decoded Level Media Mask as well: canonical `cat1` is 96x720 over a 480x3600 level background, producing exact derived 5x5 cells and exposing the recovered value-31 water classifier to `0x16880`. The canonical mask has 3,914 water cells and 65,206 non-water cells. Exact internal `0xFEE0` address arithmetic remains an instruction-closure target, not a hard-coded assumption.

The aggregate entity scanner now preserves each successful `CollisionPairResult` in traversal order, making pair-local `collisionSpawn_ID` requests visible to the world host without replaying damage. The canonical no-aim soak contains zero such requests; construction semantics remain intentionally deferred until the original spawn position/owner call contract is closed. Visible particle-system execution is also still deferred so the host does not invent a separate RNG stream or shift the shared legacy stream prematurely.

The canonical 120-tick soak now activates 3 placements, allocates 24 members, peaks at 20 active, finalizes 7 removals / 10 removal consequences / 1 consequence spawn, and has 0 entity/entity collisions / 0 surfaced collision-spawn requests in this particular no-aim input sequence. Baseline frame hashes remain unchanged; live integration hashes are `0x1eb1e07d4b6d038d` initial, `0x1e24b6143cd762ec` first air-fire tick, and `0x13c37d4b847666f9` at tick 120. macOS semantic controls are arrows/WASD, Space or Z, X, and Tab or C; original film/InputSprocket bits are still unresolved. Synthetic repository suite: **53/53 PASS**.


## 2026-08-30 — playable-host fail-fast input and live HUD correction

A device test exposed an app-boundary failure that clean-core probes could not: the macOS integration executable could continue as a bounded Preview/smoke session if `enable_live_world()` failed. That mode still displayed original Level-1 terrain and the score-bar artwork, so it could be mistaken for the game while containing no live enemies and accepting no semantic weapon actions. The macOS wrapper now treats original-data/live-world bootstrap as mandatory. Missing PAKs, load failure, live-world failure, or initial live-render failure produces an explicit startup error and terminates instead of silently presenting a non-playable fallback.

Keyboard delivery now uses an `NSWindow` first-responder subclass instead of a local `NSEvent` monitor. Arrows/WASD drive movement; Space or Z drives the selected air weapon; X drives the ground weapon; Tab or C cycles level-available air weapons. The host logs accepted air/ground launches with the live tick and constructed-member count.

The Level-1 HUD weapon cache is also corrected. Live score-bar slot 0 is the selected level-available air weapon, later slots contain only other currently available air weapons, and locked slots restore the original static panel without manufacturing a sprite. Canonical Level 1 therefore exposes Ion Cannon only rather than advertising later weapons from the definition corpus. The renderer continues to own all six recovered HUD classes: score, life symbol, life count, weapons, shield, and power.

Finally, `CollisionPairResult` now preserves the player-owner source for each damage leg. The live host routes nonzero `0x14F10` shield-depletion score awards through the already-recovered `0x29A10` player-score routine, including power multiplier and strict extra-life threshold semantics, so kill score can update the Player-1 HUD rather than remaining a decorative zero.

The HUD correction intentionally changes only the clean live-world frame witnesses: initial `0x1eb1e07d4b6d038d`, first air-fire tick `0x1e24b6143cd762ec`, tick 120 `0x13c37d4b847666f9`. Canonical static/no-input/right-control witnesses remain exactly `0x9e8a7ec73b79b254`, `0x44dede08075273f2`, `0x51d4a7eec9b0beef`, and `0x6fd5c94a64dcb0c8`. External `Game.pak` validation PASS; external full-frame/live probe PASS; synthetic suite **53/53 PASS**.

## 2026-08-30 — Playable WIP 3: bounded world, player crash lifecycle, particles, secondary fire

Native playtesting exposed four host-integration gaps that did not show up in the earlier static/live-frame milestones: long-session slowdown, missing player ram/crash effects, apparently absent secondary fire, and a misleading live HUD power meter.

The playable host now closes those gaps as follows:

- `EntityWorld::prune_finalized_history()` removes only inactive members whose recovered outer removal pass has completed, then removes genuinely empty finalized groups. This mirrors the original intrusive-list lifetime instead of retaining every dead projectile/member in portable vectors forever.
- `tick_live()` now mirrors the shipped main-tick lifetime gate instead of using a host safety bound. PPC Lab execution of original 1.0.6 routine `0x12CA0` proves the main caller passes margin **128** after movement and deletes immediately on false. Boundary sweeps close the asymmetric predicate: left uses `x+halfWidth >= -128`, right `x-halfWidth <= width+128`, top uses origin `y >= -128`, and bottom uses `y-halfHeight <= height+128`; equality survives. The gate now runs before obstacle/state/collision work, matching the original ordering.
- the recovered particle producer bridge is live for state, collision-hit and destruction producers; `0x438C0` update/prune runs every live tick and the world renderer now receives the live particle-system span.
- player-collision orchestration now consumes `LegacyPlayerDamageResult` and `LegacyPlayerPickupResult` consequences instead of dropping them. `active_SpawnOnHit`, shield-warning object, `death_Spawn`, extra-life/entry spawn, and death money-drop objects are deferred safely until collision traversal completes.
- `advance_legacy_player_lifecycle()` is now part of the live host. Dying Player 1 cannot move/fire, the ship sprite is hidden during non-active states, death timing decrements lives, respawn restores the canonical entry location/default shield, and entry effects are constructed.
- Level-1 ground/secondary fire remains the recovered Plasma Bomb chain; macOS maps both **X** and **either Shift key** to the semantic ground-fire action and logs accepted launches.
- the live score bar no longer fabricates a permanently full weapon-power channel. Until the actual power-up producer is recovered, live weapon power starts at zero; score, lives and shield remain driven by Player-1 gameplay state and air-weapon previews remain level-availability-driven.

External canonical-Pak playable-runtime validation now proves:

- deliberate opening-lane ram: first dying status at tick 185, respawn at tick 266, lives 3 -> 2, shield restored to 100, seven player-effect object spawns, and visible particle activity;
- 3000-tick continuous primary-fire / periodic-ground-fire stress after exact `0x12CA0` lifetime closure: max 114 resident/active members, 10 final resident members, 1862 finalized member records pruned and 236 original-threshold far-offscreen deletions on the current deterministic run;
- the same stress dropped from roughly 14.5 s before the offscreen bound to roughly 3.6-3.8 s headless in the current container while preserving gameplay placement boundaries.

All 53 repository tests pass. `deimos_reference_probe` passes the canonical Game.pak corpus. `deimos_original_frame_probe` preserves the four pre-live canonical frame hashes and now freezes the live witness at initial `0x1eb1e07d4b6d038d`, first-fire `0xa0fc41ac06687be2`, and tick-120 `0x055b51228f651199`, with 16 resident members / 10 resident groups at tick 120, 9 removals, 12 removal consequences, one player-effect spawn, two exact `0x12CA0` far-offscreen deletions, nine pruned finalized members, max 25 active particles, and max 19 active live members.

### 2026-08-30 live weapon-charge / HUD-power checkpoint

- The live air-weapon bridge now compiles the canonical `powerup_Air_*` fields from Weapon Definitions instead of leaving them unused.
- Level-1 Ion Cannon data drives hold-to-charge behavior: activation delay 15 ticks, activation Unit `icpo`, power-level interval 2 ticks, maximum level 20, serialized `OverloadTime` 180, release Unit `icps`, release cadence 1 tick. Direct PPC Lab execution of shipped handler `0x3B3C0` proves `OverloadTime` does **not** force an automatic release in this handler.
- The recovered score-bar power meter is now fed from live charge percentage rather than being held at a deliberately neutral zero.
- Charged release transitions the matching player-owned activation Unit into the state marked `stateUseThisStateOnWeaponPowerupRelease_BOOL` and schedules canonical release-spawner Units.
- The player-owned Lock-to-owner bridge is intentionally restricted to the selected weapon's activation Unit; broad player-owner locking was rejected because it perturbed established live-world oracles.
- `deimos_playable_runtime_probe` now checks charge activation/release in addition to crash/respawn, secondary fire and long-run object bounds.
- Current playable stress with continuous primary charge plus periodic Plasma Bomb: max resident 111, final resident 18, 2,264 finalized members pruned, 269 far-offscreen members culled on the reference host.
- Baseline live-frame integration hashes remain unchanged for initial, first ordinary fire and tick 120.

Fidelity note: shipped handler `0x3B3C0` is now instruction-closed for this charge/release slice. It emits one `ReleaseSpawn_ID` per attained power level at `TimeBetweenReleaseSpawns`, decrementing the stored level once per emitted spawner. `DoReleaseOnMaxPowerLevel` is the proven max-charge automatic-release switch; the serialized `OverloadTime` field is preserved but is not consumed as an automatic-release timer by this handler.

### 2026-08-30 PPC WIP5 hit/pickup feedback closure

- Shipped sprite/player feedback helpers `0x12BC0` / `0x12C10` are now live. Canonical collision/coin pulse is white (`0x7FFF`), rate 6, and follows raw legacy effect amounts `32 -> 26 -> 20 -> 14 -> 8 -> 4 -> 10 -> 16 -> 22 -> 28 -> 32/off`.
- Entity/entity damage triggers the recovered collision-glow pass when the pre-hit state permits it; accepted coin pickup triggers the same pulse on Player 1.
- Player-vs-entity impact no longer discards its returned `CollisionDamageResult`: ram hits now consume entity glow, instruction-closed collision-spawn requests, and player-attributed score before reciprocal player damage continues.
- Regression remains 53/53 PASS; all original/static/live frame hashes remain unchanged; playable crash remains dying@185 / respawn@266, Plasma Bomb and charge gates pass, and the 3000-tick stress remains bounded at maxResident 114 / finalResident 10 / pruned 1862 / exact culls 236.


### 2026-08-30 WIP6 secondary / respawn / reticle closure

- macOS left/right Shift now reaches ground fire through AppKit `flagsChanged:`; X remains a ground-fire key.
- the canonical Plasma Bomb gate proves real ground damage (`bsde` shields 4.0 -> 3.6), not merely request construction.
- state visuals enter at `stateSpriteFrameMin`, so Shield Warning `nosw` uses `noti` frame 4 rather than GET READY frame 0.
- player death mirrors shipped `0x27E50 -> 0x34B90` owner cleanup before death consequences, preventing player-owned warnings/effects from surviving their owner when the state requests destruction/deletion.
- the canonical `pbta` ground reticle is persistent on layer `defa`, anchored at player plus serialized offset `(0,-121)`, normal frame 0 and locked frame 1 on eligible ground-target overlap.
- framebuffer verification across ticks 266..320 confirms the stale GET READY presentation is gone after respawn.
- live-only frame witnesses with restored reticle: `0xbdf7558de9357ff7`, `0x036bb03279ae5b48`, `0x8e4063956c4df5cc`; static witnesses are unchanged.
- repository synthetic suite: **53/53 PASS**.

### 2026-08-30 — WIP7 front-end/control restoration

The playable macOS host no longer drops directly into combat with undocumented controls. Original 1.0.6 resource-fork evidence closes the Player-1 keyboard defaults (Arrows, Option=Air, Command=Ground, Space=Select) and the shipped Preferences/Controls surface. The native host now has launch, pause, Game/View/Help menus, control discovery, restart, fullscreen access, and preference documentation while pausing the simulation safely. Modern WASD/Z/X/Shift/C/Tab aliases remain available. Exact classic DLOG/DITL artwork rendering and active audio/gamepad preference backends remain open.

## 2026-08-30 — WIP8 animation/orientation + AI ordering closure

The live entity layer now implements the recovered `0x146F0`/`0x15930` state-animation block instead of treating sprite frame and `Animation Stopped` as static presentation data. Directional states initialize from physical/editor heading, `0x16230` heading-to-frame mapping is regression-tested, finite animation can drive `This Entity's Animation Has Stopped` rules in the same tick, and `0x172D0` RotateToTarget changes visual direction without mutating physical heading. Static atlas selectors still honor serialized `stateSpriteFrameMin`, preserving the WIP7 Shield Warning / GET READY correction.

The main live-member host now places movement/lifetime after animation/rules/target/motion, retains owner Lock/Link/Orbit before spawn scheduling, and moves ground-obstacle stopping to the proven later slot after due child spawn requests are built. Delayed groups now implement the shipped `1 -> 0` same-tick activation rule. `statePauseVerticalScrolling_BOOL` is OR'd into a following-frame terrain-scroll latch; long Level-1 encounter holds reappear in the same regions found during PPC research.

The ground-placement audit rejected a global coordinate correction. Level placements deliberately subtract the terrain world origin during construction, whereas child absolute spawn sets deliberately start from zero and leave the constructor subtract flag clear; relative/rotated child spawns use the recovered parent-based geometry. Moving all ground objects would break that binary-confirmed distinction.

Regression investigation showed the old dying@185 / respawn@266 playable timing is not stable once the previously missing animation/orientation/Animation-Stopped layer executes: full WIP8 is dying@171 / respawn@252. Disabling animation while retaining WIP8 ordering yields 184/265; additionally restoring the WIP7 delay gate yields exactly 185/266, isolating the shift rather than blindly accepting it. Static preview hashes are unchanged. WIP8 live-only hashes are initial `0xcd72678207b195b7`, first air-fire `0x800f06651d29406a`, tick-120 `0x267609db3ba6dbcc`; the 120-tick probe ends at 15 resident members / 9 groups and peaks at 18 active. See `docs/WIP8_ANIMATION_AI_ORDERING.md`.

Final WIP8 post-documentation freeze gate: repository rebuild PASS; synthetic suite **53/53 PASS**; canonical Game.pak clean-core probe PASS; original-data frame probe PASS with unchanged static hashes and live `0xcd72678207b195b7` / `0x800f06651d29406a` / `0x267609db3ba6dbcc`; playable-runtime probe PASS at dying@171 / respawn@252, Plasma Bomb `bsde` 4.0 -> 3.6, Ion Cannon activation=15 ticks / `icps` release, and stress3000 maxResident=96 / finalResident=27 / maxActive=96 / pruned=1871 / farCulled=213.

## 2026-08-30 — WIP9 flee targets / target-motion / enemy-fire heading closure

The original 1.0.6 PowerPC PEF was recovered again from the canonical StuffIt -> SMI ->
HFS evidence chain (2,045,976-byte data fork, SHA-256
`8e436c3babc582f1407ae6fed47e9749f1c930335ce4c794947e40b06b85eb29`) and used to
close the remaining shipped target/flee edge behavior before changing the clean runtime.

PPC `0x17510` proves `stateFlee_ID` selects authored destinations, and `0x16CC0` accelerates
toward those destinations using the flee speed/delta pair. Canonical Game data contains
17 explicit flee states, eight north-on-no-player Unit Definitions and one south-on-no-player
Unit Definition. State entry consumes flee-target RNG before spawn-runtime RNG; entering a
flee state through the range handler installs the target immediately but does ordinary
convergence for the rest of that tick, entering the flee early path on the next tick.

PPC `0x161C0` proves rotated child-spawn geometry derives heading from the current sprite
frame/direction geometry rather than the stale construction heading. The existing
`0x15B40` / `0x17CB0` scheduler was re-audited and remains the canonical enemy-fire cadence
mechanism; no host-side firing-rate tuning was added. In the deterministic 3000-tick WIP9
soak, 1,092 rotation-adjusted spawn events occur and 48 use a visual heading different from
the construction heading. Twenty-three explicit flee activations are exercised.

WIP8's early oracles remain unchanged: static hashes, live initial/fire/tick120 hashes, and
dying@171 / respawn@252 all survive WIP9. The long-run bounded-world profile changes to
maxResident 84 / finalResident 15 / maxActive 84 / pruned 1773 / farCulled 136. A four-way
temporary build matrix isolates that shift completely to the two PPC-backed WIP9 changes;
restoring both WIP8 paths reproduces 96/27/1871/213 exactly. See
`docs/WIP9_FLEE_TARGET_FIRE_HEADING.md`.
