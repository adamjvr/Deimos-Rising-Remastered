# Research Log

## 2026-08-27 — multi-build corpus intake and first full extraction

### Mac 1.0.6

`DR-EVID-002` was decompressed through StuffIt method 15 (Arsenic/BWT), including its resource fork. The resulting 81,788,928-byte self-mounting image is a Classic Macintosh HFS volume named `Deimos Rising 1.0.6`.

The HFS catalog was extracted with no recorded extent problems. The install contains the PowerPC PEF application, resource fork, registration application, HID bundle, Player Guide, local-override directory tree, and four game PAKs.

### PAK/resource model

The PAKs are ZIP archives. The application emits diagnostics naming `Data:Paks`, `Data:Local`, and `U_Pak.cc`, states that only ZIP files are supported in Paks, and that ZIP files are not supported in Local. This strongly establishes a loose-local-overrides + packaged-resource architecture.

Observed loose directories are:

`coli`, `film`, `flli`, `idli`, `im08`, `im16`, `leve`, `plde`, `pref`, `reli`, `soun`, `stli`, `tefo`, `unde`, `wede`.

PAK totals:

- Audio: 96 files — all `.IMA` (AIFC/IMA4).
- Game: 763 actual files — 386 `unde`, 248 GIF, 54 `tefo`, 38 TGA, 12 `leve`, 6 `idli`, 5 `stli`, 5 `wede`, 4 `film`, 2 `plde`, and singleton `coli`, `flli`, `reli`.
- Interface: 9 actual files — 7 TGA + 2 GIF.
- Music: 3 files — one `.aif` and two `.IMA`, all AIFC/IMA4.

The 12 canonical level resources are `[le01]` through `[le12]`. Four canonical demos are `[de01]` through `[de04]`.

### Asset plate semantics

Mod documentation points replacements at `Data/Local/im08`. Image pairs use names such as `Player 1 Blue IA[PL1B].gif` and `Player 1 Blue IC[pl1b].gif`. The original application diagnostic `Sprite color and alpha plates are not equal size` independently corroborates that these are paired color/alpha plates.

### PEF executable

The main PEF imports ten libraries: MathLib, QuickTimeLib, AppearanceLib, InternetConfigLib, DrawSprocketLib, InterfaceLib, SoundLib, UnicodeConverter, TextCommon, and InputSprocketLib, totaling 445 imported symbols.

The executable retains unusually rich diagnostic strings, original implementation filenames, and serialized-field names. These provide independent naming anchors for reconstructing systems including levels, films/replays, sprites/images, resource PAKs, level selection, input, and audio.

### Add-ons / Apple Bundle updater

The add-ons archive contains an Apple Bundle 1.0.6 updater. Its Read Me states that it updates Apple-bundled copies from v1.0.0 through v1.0.5 and is not intended for shareware copies. Reported changes include OS X window/cursor/HID behavior, windowed fades/dissolves, high-score speed, locked-volume handling, Panther compatibility, fullscreen transition handling, preferences location, and error reporting.

The add-on corpus also exposes ten declared 40,296-byte demo films, multiple player/sprite mods, MP3 soundtrack files, original theme-song archives, desktop art, and player-guide/reference imagery.

## 2026-08-28 — tagged-text, PAK runtime, level, and film breakthrough

### Legacy data transform

The apparent binary resources were decoded as seven-bit ASCII under a reversible byte transform. A corpus pass decoded all 473 canonical `leve/unde/plde/wede/idli/flli/coli/tefo/stli/reli` resources without replacement characters. The clean core now implements the exact decoder and a deliberately non-historical canonical inverse for synthetic fixtures.

The transform also exposed corrected identities for previously tentative buckets: `flli` is Float List, `tefo` is Text Format, `stli` is String List, and `reli` is Rect List.

### Data-driven game model

The 386 unit definitions contain extensive declarative state-machine, movement, sprite, sound, spawn, shield/damage, and rule data. Repeated-key counts identify 1,167 state records and 5,835 state-rule records across the canonical corpus. This shifts the clean-room strategy toward reconstructing the state-machine interpreter rather than hard-coding 386 entity classes.

### Levels

All 12 levels share one strict 11-field header followed by seven-field object placements. The 565 declared object placements reconcile exactly. A typed clean C++ loader now validates the original levels.

### Replay films

The Perfect Demos BinHex/StuffIt material yielded eleven additional 40,296-byte v10005 replay films. Combined with the four canonical PAK demos, 15 recordings provide 135,840 active input ticks. Each tick carries a seven-bit mask; two bit pairs have opposing-direction statistical signatures and a rare seventh bit is consistent with documented weapon switching, but exact action names remain unassigned pending stronger executable/two-player evidence.

### Runtime resource access

All four canonical PAKs use only ZIP method 0/stored members. A dependency-free clean PAK reader with CRC32 validation and a Local-over-PAK `ResourceStore` was implemented. It successfully CRC-validated all 871 actual original files and parsed all original levels/replays directly from `Game.pak`.

### Text-format type correction

Real-corpus validation showed that `tefo` field `Format_ID` is polymorphic at the text level: values include the four-character tokens `LEFT`, `CENT`, `CEBU`, `RIGHT` and the one-character values `3` and `4`. The clean parser preserves this as an opaque token rather than assuming the `_ID` suffix implies a FourCC.

## 2026-08-28 — entity transition interpreter recovered

### Exact string/action semantics

The common PPC comparator at code `0x57820` was disassembled and is a plain
byte-exact `strcmp`. This corrected an earlier case-folding assumption. State
actions in `0x146F0` recognize exact `Delete`, exact `Destroy`, or an exact local
state name. Any other non-empty label returns without changing state.

Re-running the complete canonical corpus under exact lookup changes the active
unresolved/no-op count from 29 to **44**. The additional 15 are all the literal
`Wait for Player Approach`, while the corresponding state is
`Wait For Player Approach`. They occur as `Is Active` rule actions and are
preserved as original no-ops rather than repaired silently.

### Full 17-condition rule dispatch

Rule evaluator `0x15550` contains a 17-way dispatch table even though canonical
1.0.6 data uses only nine non-empty condition strings. Handler semantics were
mapped for tracking, active/inactive, destroyable-air/ground presence, player
presence, player range, animation stop, required visibility/tint/scale, and
unit-count equality/less-than/greater-than.

The five rule slots execute in file order and the first true predicate wins.
The evaluator exits after calling the state-action routine even when that action
is an unresolved/no-op label.

### Timer RNG and exact tick trigger

Helper `0x46580` maps the 15-bit RNG result from `0x553E0` with
`min + rng % (max-min+1)`, using the endpoint directly when `min == max`.
The base RNG is the 32-bit LCG update `seed = seed * 1103515245 + 12345` and
returns bits 16..30.

State entry stores the current tick and chosen delay. The main entity update
fires the timer only on exact equality with `entryTick + delay`, not when the
current tick is later. All 1,167 canonical timer bound pairs satisfy `min <= max`.

### State-entry counter

The entity structure contains 20 per-state entry counters. State transition
increments the entered state's counter and immediately compares it against
`stateOnCounter_INT`. The largest canonical threshold is 16. A non-empty state
transition attempt resets the current slot before recursively entering the
new state; this reset also occurs before an unresolved non-empty label becomes
a no-op.

### Range transition and update ordering

The range handler at `0x15280` treats exact `0.0f` as disabled and otherwise
uses strict `measuredDistance < configuredRange`.

The main entity path around `0x33C58` establishes transition ordering: timer
first, then animation update, then rules, later range processing. Delete/destroy
results exit the normal update path at each stage.

A dependency-free clean `state_runtime` kernel and complete 17-condition rule
predicate layer now encode these confirmed semantics.


## 2026-08-28 — spawn scheduler, geometry, and target gate recovered

### Scheduler and RNG ordering

PPC `0x15B40` is the live per-entity spawn-set scheduler and `0x17CB0` initializes spawn runtime on state entry. Each spawn set owns a 24-byte runtime record containing selected rate, anchor tick, remaining/initial volley counts, per-entity delay, and an active byte. State entry consumes RNG in rate -> volley -> delay order, while repeat re-arm consumes delay -> volley -> rate. The differing call order is preserved because it changes the global RNG stream and therefore replay behavior.

The original inclusive mapper at `0x46580` uses signed PPC remainder without normalizing reversed endpoints. Canonical unit `Level 8 - Mid 2[08m2]` contains the malformed range 110..20; the executable consequently produces 110..198. The clean runtime was corrected to preserve this behavior.

### Direct Unit Definition memory mapping

The Unit Definition parser at `0x3FDA0` exposes literal key strings and destination addresses. Three spawn-relevant booleans are now directly proven:

- compiled `+0x12A` <- `#canBeSpawnedOnlyWhenPlayersActive_BOOL`;
- compiled `+0x12E` <- `#adjustInitialLocForOwnerScale_BOOL`;
- compiled `+0x132` <- `#terrainEffect_BOOL`.

This corrected an earlier tentative interpretation of `+0x132`. The spawn gate at `0x15D8C` reads `+0x132`, so it is definitively terrain-effect eligibility: terrain-effect targets are rejected when the parent is stationary or when the parent entity's inherited terrain-effects option is disabled.

### Geometry and original trigonometric tables

The geometry tail at `0x15E18..0x16158` reconstructs relative/absolute coordinate behavior, optional owner scaling, heading adjustment, rotation, and PPC `fctiwz` truncation. Startup routine `0x42920` builds 360-entry single-precision sine/cosine tables from embedded float bits `0x3C8EFA35`; lookup helpers are `0x42EE0`/`0x42F00`. The clean core preserves the exact input constant, float rounding boundaries, fused operation ordering, and heading-360 lookup behavior. Host `sin/cos` is currently used for final table generation pending measurement against classic Mac MathLib.

### Clean request boundary

A scheduler `spawn_due` event can now be converted into a portable spawn request seed after resolving the target Unit Definition. The seed contains target FourCC, position, optional heading, and inherited child stationary/terrain flags. Parent pointers/owner IDs and remaining `0x33220` entity-construction behavior are deliberately deferred rather than guessed.
## 2026-08-28 — Entity construction / initial member runtime

- Directly disassembled `0x33220`, `0x35BF0`, `0x35CD0`, `0x369F0`, `0x36AB0`, `0x37930`, `0x37B50`, and float RNG `0x465E0`.
- Replaced the provisional one-request/one-entity abstraction with a group/container plus surviving live-member model.
- Confirmed group/appearance RNG precedes the player-active and duplicate/cap gates.
- Mapped the 44-byte request's owner and parent fields to a signed player-owner byte and pointer+serial safe parent reference.
- Recovered independent group/member serial counters and first-member output-reference behavior.
- Implemented original initial-position asymmetries, float RNG, canonical initial-speed/heading paths, state-zero initialization ordering, and cumulative group delays.
- Real `Game.pak` validation: all 386 Unit Definitions pass initial-member math; normal shared-RNG probe constructed 386 groups / 546 live members.
- Canonical `Mine[mine]` is the only initial-hunt definition; `Screw Mk 2[sc02]` contains the only reversed X/Y offset range (`100..0`).


## 2026-08-28 — World registry and owner-location runtime

Direct PPC disassembly mapped the first post-construction world layer. Safe
entity reference validator `0x36AB0` requires pointer/handle, matching live
serial `+0x9C`, and active byte `+0xCB`. Duplicate scan `0x36AF0` searches active
members by Unit FourCC; owned-type scan `0x36BE0` additionally matches signed
player owner `+0xD8` before entering the member-removal path.

State parser/runtime correlation proved `stateLockToOwnerLoc`,
`stateLinkToOwnerLoc`, and `stateOrbitOwner` at compiled-state `+0x32E/+0x32F/+0x330`.
Initializer `0x33600` is called from both construction (`0x35FB4`) and normal
state changes (`0x14D74`). Tick routines are `0x37130` Lock, `0x37230` Link,
and `0x37350` Orbit, called in that order immediately before spawn scheduling.

Canonical corpus counts are 156 Lock states across 71 units, 10 Link states
across 5 units, and 8 Orbit states across 4 units. The clean world registry
validated all 546 live members produced by the existing shared-RNG constructor
probe.

Orbit exposed another reusable legacy-math contract: startup generates a
1,024-entry integer atan table from exact doubles 0.01 and 57.2957795;
`0x43090` performs the table/quadrant conversion. Clean code now implements
that contract rather than a generic host atan2 call.


## 2026-08-28 — Collision candidate scan and shield/damage runtime

Re-entered the canonical Mac 1.0.6 PEF and disassembled collision scan
`0x36CF0`, AABB helper `0x12AD0`, damage routine `0x14F10`, ordinary destruction
`0x16300`, and their Unit/state parser consumers. This tied the source-format
collision fields directly to compiled offsets and proved the loader-derived
`grnd` / `air ` collision domain from `isGroundBased_BOOL`.

The scan is not a generic all-vs-all AABB pass. It filters by active current
collision state, collision-participation/visibility byte, serial, matching
domain, **opposite** `harmlessToPlayers` class, group delay, and an asymmetric
player-projectile policy before integer AABB and the then-unresolved routine
`0x42F80`. Bounds are produced with per-edge `fctiwz`, and touching edges pass.
Subsequent analysis (documented below) proved `0x42F80` is a quantized radial
center-distance test, not sprite/mask geometry, and the temporary callback was
removed.

A radial hit performs two damage legs. Owner redirection uses validated safe
parent references. The second leg contains an observable 1.0.6 oddity: when
candidate `passHitsToOwner` is true, code at `0x37074` loads self's parent pair
rather than candidate's. This is preserved as compatibility behavior.

Damage `0x14F10` uses canonical `Entity_HitDelay=1.0` as a strict
`currentTick > lastHit+delay` gate, clamps shields at zero, restores old shields
for collision-invulnerable states, and uses a separately delayed on-hit state
action. Review of register lifetime exposed another ordering detail: the
compiled-state pointer is captured before the on-hit transition and retained,
so same-call glow and collision-spawn fields come from the pre-hit state. A
regression test now locks that behavior. Collision-spawn delay fires on equality
(`>=`) and non-repeat spawns use per-member bookkeeping.

The clean runtime records ordinary shield-depletion destruction, score/source
owner, and deterministic side-effect facts without pretending the full `0x16300`
consequence graph is complete. At this checkpoint the remaining fronts were
level-scaled shield initialization, player/ground/terrain collision, special
destruction, death effects, child/terrain and group-kill/removal consequences;
the radial/player/shield portions were resolved in the subsequent pass below. Canonical
`Game.pak` validation reports 436 collision-enabled states and 135 air / 251
ground unit domains; the pre-existing constructor and first-tick RNG regressions
remain unchanged.


## 2026-08-28 — Radial collision, player impacts, pickups, and shield scaling

Re-analysis of canonical Mac 1.0.6 routine `0x42F80` disproved the earlier
sprite/mask hypothesis. The routine subtracts the two center positions in
single precision, computes `dx*dx + dy*dy`, converts that squared distance with
PPC `fctiwz`, then compares the resulting distance against the sum of two
radii with strict `<`. Startup `0x429C0..0x42A00` constructs the fast table for
inputs below 16384 by storing `frsp(sqrt(float(i)))`; larger inputs take the
sqrt path directly. The clean compatibility expression is therefore:

`float(sqrt(trunc(single(dx*dx + dy*dy)))) < radius1 + radius2`

Entity/entity radii are integer `(maxY-minY)/2` from the already-truncated AABB.
The player path uses the same helper but computes player radius as
`0.5f * (Rect.bottom-Rect.top)`, so it can be a half-integer. This exposed a
useful regression edge: raw center distance 6.5 with radius sum 6.5 still hits,
because 42.25 is truncated to 42 and `sqrt(42) < 6.5`. At 6.56 the squared
value truncates to 43 and the collision fails. The test suite locks both cases.

The main member tick player-impact path `0x34090..0x34314` is now bounded in
clean code. It requires an active current state with `stateCollides_BOOL`, a
non-harmless Unit Definition, and `stateCollidesWithPlayers_BOOL`. Before the
two-player loop it applies the original viewport guard: entity `maxX >= -32`,
`minX <= r24`, `maxY >= 0`, and `minY <= r23`. Player active flags, Rects,
centers, and radii are snapshotted before the loop. After AABB + radial overlap,
ordinary entities take Game.gafl index 161 (`Player_ImpactDamageToEntities`,
canonical value 100.0), optionally redirected through `passHitsToOwner`; the
player then always receives the colliding entity's UnitDef `damage_FLOAT`, even
when the first leg just destroyed the entity or its owner. Player status is
re-read afterward, preserving the original status-4 active contract.

The special branch at `0x341D0` is now identified as pickup handling. UnitDef
`+0x4D4` is the pickup category corresponding to `pickup_Type_ID`, with
`+0x4DC` supplying `pickup_Value_INT`. `0x37580` dispatches legacy pickup
categories including the canonical `coin`, `mult`, `shie`, and `exli` types.
A non-`none` pickup never falls through to ordinary impact: failed pickup does
nothing further; successful pickup invokes destruction/consumption with the
player's signed index/owner byte and skips reciprocal damage. Inventory/weapon
mutation remains an explicit callback until the player subsystem is recovered.
Canonical Game.pak contains 8 pickup Unit Definitions: 4 coin, 2 shield,
1 extra-life, and 1 multiplier.

Shield construction was recovered independently at `0x35E50..0x35EB0`.
The constructor copies `shields_BaseAmount_FLOAT`; only when
`shields_LevelIncrement_FLOAT > 0` does it add
`increment * (gameContext+0x14 - 1)` and then clamp to
`shields_MaxAmount_FLOAT`. A non-positive increment does not apply the max
clamp. The semantic name of game-context `+0x14` is deliberately left
unasserted, but the arithmetic and branch behavior are now exact.

Canonical validation after these changes remains stable: 386 groups / 546
members constructed with RNG seed 2249411936; the first player-aware tick leaves
544 active members with motion RNG seed 2633739833. The repository suite remains
21/21 PASS.

Destruction follow-on reconnaissance also narrows the next boundary. Routine
`0x16300` uses UnitDef `+0x478` as the destruction spawn ID, `+0x47C` as the
destruction-particle ID, the packed field at `+0x480` as particle color,
`+0x482` as destruction notice text, `+0x4A4/+0x4A8/+0x4AC` as destruction
coin count / coin ID / group-kill coin ID, `+0x4B0/+0x4B1` for child-destruction
handling, `+0x4B8` as score, and `+0x4BC` as the beginning of the destruction
sound descriptor. The exact semantics/order of the remaining boolean cluster
`+0x4B2..+0x4B4` are being held at bounded confidence until the terrain,
obstacle, and random-bonus callees are all correlated; clean code should not
name those fields prematurely.


## 2026-08-28 — Destruction, group removal, and random-bonus runtime

PPC `0x16300`, `0x36120`, child helpers `0x363C0` / `0x364F0`, and the outer
inactive-member cleanup around `0x36610` were traced as a single two-stage
teardown system rather than collapsed into a generic "delete entity" action.
The clean runtime now preserves that split: immediate ordinary destruction
side effects can occur at the lethal call site, while group/list removal and
counter consequences happen later and are idempotent with respect to effects
already emitted.

The Unit Definition offset cluster around the destruction fields is now named
from exact `.unde` loader keys rather than correlation alone. In particular,
`+0x4B2/+0x4B3/+0x4B4` are `destructCreateObstacle_BOOL`,
`destructDrawToTerrain_BOOL`, and `destructReleaseRandomBonus_BOOL`.
`+0x478/+0x47C/+0x480/+0x482` are destruction spawn, particle, particle color,
and notice; `+0x4A4/+0x4A8/+0x4AC` are ordinary coin count/ID and group-kill
coin ID; `+0x4BC..+0x4D0` is the complete destruction sound descriptor.
`+0x2DC` is the deletion-only spawn. State-relative `+0x329/+0x32A/+0x32D`
map to child-destroy opt-in, child-delete opt-in, and destroy-owner-on-destruction.

Group fields are now independently proven: `+0xA4` is the original member
count, `+0xA8` the current active count, and `+0xAC` the destroyed-member
counter. Group kill is detected when destroyed count reaches the original
count, not when the active count merely reaches zero. Ordinary and group-kill
coin rewards require player-attributed destruction and are suppressed for the
pickup-consumed marker at live `+0xCA`. Group FourCC `SERM` is explicitly
exempt from ordinary group-removal behavior.

Child propagation also preserves a non-modernized original detail: the helper
matches the child's stored **parent serial** against the owner's serial and does
not first validate the pointer half of the safe reference. Per-state flags then
decide whether that child participates in destruction or deletion propagation.
The outer cleanup separately handles validated parent destruction, deletion
spawns, obstacle conversion, and terrain-draw requests.

The random-bonus path was closed against fixed positional resources rather than
inventing a data-driven replacement. `Game[gafl]` 209..219 are label-verified
as the nine cumulative percentage thresholds plus ground-accuracy threshold and
minimum-progression value. `Objects[gaob]` 25..34 are label-verified as
`RandomBonus_1..10` and resolve to `rb01..rb10`. Canonical thresholds are
70,78,82,84,87,91,95,98,100; the special threshold is 10 and minimum progression
is 3. Loader float values are converted with the original truncation behavior.
The >=98 tail gates `rb09/rb10` on game-context `+0x14`; that context field's
higher-level name remains deliberately unresolved.

Collision integration exposed exact immediate ordering: an ordinary lethal
`0x14F10` call reaches `0x16300` at the call site, so random-bonus selection can
consume RNG before later cleanup. Successful pickup likewise invokes `0x16300`
before writing the pickup-consumed marker. Clean collision callers can now
supply a removal context to reproduce this order without double-emitting effects
when the later group teardown runs.

A synthetic-fixture regression found an implementation-only sentinel edge:
an all-zero `FourCC{}` must be treated as absent alongside serialized `none` and
`NULL`; otherwise clean tests can emit a phantom resource even though canonical
Unit Definitions normally carry an explicit sentinel. The destruction runtime
now normalizes all three absent forms.

Canonical Game.pak cross-checks currently report 99 destruction-spawn units,
99 destruction-particle units, 77 destruction sounds, 28 ordinary coin-reward
units, 15 group-kill reward units, 54 destroy-children units, 58 delete-children
units, 13 obstacle creators, 32 terrain-draw units, and 7 random-bonus units.
At that destruction/group checkpoint the repository suite was 22/22 PASS. Shared construction remained 386 groups / 546 members with RNG seed 2249411936, and the first player-aware tick remained 544 active with motion seed 2633739833. The next edge at that point was `0x16880`; it is resolved by the subsequent terrain/media entry below.


## 2026-08-28 — Terrain/media removal routing and ground-obstacle store

Re-entered PPC `0x16880`, `0xFEE0`, `0x2A6D0`, `0x2A770`, `0x2A830`, `0x2A950`, and the destruction outer-cleanup obstacle path. Parser correlation proved UnitDef `+0x11E/+0x125/+0x128/+0x12B/+0x2E4` as `castsShadows_BOOL`, `isGroundBased_BOOL`, `collidesWithGroundObstacles_BOOL`, `doDeathSpawnOnAnyMedia_BOOL`, and `mediaImpactSize_ID`.

`0x16880` is not a passive gate. For ordinary ground entities it samples the Media Mask at `(trunc(x)+32, trunc(y)+worldYOrigin)`. `0xFEE0` recognizes mask value 31; on that water path the caller's requested destruction/deletion spawn is suppressed, and `mediaImpactSize_ID` may cause `0x16880` to emit a water-impact replacement. `Objects[gaob]` slots 6..9 are label-verified Water Tiny/Small/Medium/Large resources (`spti/spsm/spme/spla`). Random `smra/mera/lara` selectors preserve their exact original branch ordering and shared RNG draw.

The background module's 16-byte Rect list is now reconstructed as a persistent ground-obstacle store: append without merge (`0x2A6D0`), vertical top/bottom shift (`0x2A770`), inclusive edge overlap (`0x2A830`), and reset (`0x2A950`). `destructDrawToTerrain_BOOL` appends destroyed ground-unit rectangles to this same store, while the main member tick queries it only for `collidesWithGroundObstacles_BOOL` units. This corrects the earlier loose "invalidation" wording; the list demonstrably affects later collision.

`destructCreateObstacle_BOOL` remains a separate conversion. Outer cleanup sets live render/obstacle bytes, copies `castsShadows_BOOL`, and calls `0x12F20`. The clean trace now preserves the rect and shadow flag, but exact renderer-record/pixel mutation remains deliberately outside the claimed subset.

The clean suite increased to 23/23 PASS. Canonical `Game.pak` reports 67 shadow casters, 4 ground-obstacle colliders, 12 any-media death-spawn units, and 3 non-`none` media-impact units; construction and first-tick deterministic seeds remain `2249411936` and `2633739833`.

## 2026-08-28 — Ground-obstacle stop correction and concrete player runtime

Re-entered the main entity tick around `0x34504..0x34578` and resolved the
shared two-float source previously misidentified as a rollback snapshot. PEF
relocation and cross-reference analysis prove it is the engine's canonical
`{0.0,0.0}` vector, reused by constructor/motion code. A successful `0x2A830`
ground-obstacle overlap therefore leaves x/y unchanged, copies zero into live
velocity `+0x10/+0x14`, and sets live `+0x13C` stationary. If
`destructDrawToTerrain_BOOL` is set, the current Rect is appended to the same
persistent obstacle list afterward. The surrounding live `+0x19 == 0` gate is
preserved as a bounded main-tick condition rather than assigned an unsupported
semantic name.

Mapped pickup dispatcher `0x37580` and its direct player callees. Canonical
pickup types are exactly 4 `coin`, 1 `mult`, 1 `exli`, and 2 `shie`. `coin`
adds nonzero `pickup_Value_INT`; `mult` follows `1->2->3->4->5->10`; `exli`
increments below the PlayerDef max and emits `life_Spawn_ID`; `shie` adds and
clamps semantic shield to `[0,100]`; executable-retained `air `/`grnd` branches
reject while live player `+0xCE` invulnerability is set. `spec`/default are
accepted no-ops.

Mapped player damage `0x27100`: status 4 only, `current >= lastHit+delay`, hit
tick written before invulnerability, incoming damage multiplied directly by
PlayerDef `shieldBaseHitPercentage`, no shield clamp, strict negative-shield
death, hit glow even on invulnerable accepted hits, rate-limited
`active_SpawnOnHit_ID`, and one-shot low-shield warning latch. Immediate death
helper `0x27E50` emits `death_Spawn_ID`, clears hit bookkeeping, decomposes held
money 50/10/5/1 through fixed `Objects[gaob]` 2..5 (`calg/cals/casg/cass`),
clears money, sets status 3/current tick, and raises invulnerability. It does
**not** decrement lives; that remains in the downstream death/respawn state
machine.

Added label-verified player runtime contracts for `Game[gafl]` 161/162/167 and
`Objects[gaob]` 2..5. The clean suite is now 24/24 PASS. Canonical `Game.pak`
still produces 386 groups / 546 members, construction RNG seed `2249411936`,
544 active after the first player-aware tick, and motion RNG seed `2633739833`.

## 2026-08-28 — Player death/respawn lifecycle and compiled PlayerDef layout

Traced the downstream player status switch at PPC `0x2A150` and its respawn
initializer `0x29CC0`, keeping immediate death entry `0x27E50` as a separate
stage. Status values are now bounded as 1 = game-over countdown, 2 = waiting /
entry delay, 3 = dying, and 4 = active. Every lifecycle duration test uses the
original signed 32-bit tick arithmetic with strict `currentTick > statusSince +
duration`; equality still waits.

The Player Definition's compiled memory order is not the serialization order.
Cross-references prove `gameOverTime/dyingTime/finalDyingTime` at
`+0x80/+0x84/+0x88`, `entry_InvulnerabilityTime` at `+0x8C`, solo entry x/y at
`+0x90/+0x94`, multiplayer entry x/y at `+0x98/+0x9C`, `entry_Spawn_ID` at
`+0xA0`, `entry_InitialDelay` at `+0xB8`, and the independently proven
`death_Spawn_ID` at `+0xBC`. This corrects the earlier provisional claim that
entry invulnerability occupied `+0xB8`.

Status 3 chooses `finalDyingTime` only when the semantic life count is exactly
one; otherwise it uses `dyingTime`. The fifth argument to `0x2A150` controls
whether expiry consumes a life. Its direct caller supplies a global byte that
latches to one after Player 1 first reaches active status 4, bounding the flag's
meaning as a gameplay-start latch. After optional decrement, remaining lives
call `0x29CC0`; zero lives enter status 1 and preserve player enabled `+0xC4`
until a later strict `gameOverTime` expiry clears it.

`0x29CC0` selects PlayerDef solo or multiplayer entry coordinates based on live
`+0xCD`, writes x/y, and copies live velocity `+0x10/+0x14` from a shared
relocated executable constant. PEF TOC reconstruction resolves that constant to
exact float bits `{0.0f,0.0f}`. This is direct binary evidence that the
serialized `entry_StartVelocity*` values are not written by this respawn path.
The initializer then enters status 4 at the current tick and requests
`entry_Spawn_ID`; the returning status-3 branch restores default shield and
clears the shield-hit, hit-spawn, and warning bookkeeping.

Active status-4 invulnerability uses `entry_InvulnerabilityTime` at `+0x8C` and
has two independent blockers before clearing: external gate `0x5CF0` and live
`+0xCF`. The clean API keeps `0x5CF0` as an explicit bounded orchestration input
rather than inventing its higher-level meaning.

A dedicated lifecycle regression raises the repository suite to 25/25 PASS and
covers strict equality boundaries, solo/multi entry selection, zero-velocity
respawn, ordinary/final-life timings, gameplay-start-gated life consumption,
game-over disable, and both invulnerability blockers.

## 2026-08-28 — Entity core-edge flags: air-domain obstacle gate and shield-depletion state

Direct constructor disassembly closes live `+0x19`: `0x35F88..0x35FA0` compares the derived UnitDef collision-domain FourCC at `+0x08` against `air ` and stores the Boolean result. Main tick `0x344F8` checks that cached byte before `collidesWithGroundObstacles_BOOL`, proving air-domain members never enter the persistent ground-obstacle Rect query. The clean terrain query now enforces the same domain gate.

State parser `0x41698..0x416A8` writes the Boolean key `stateUseThisStateOnShieldDepletion_BOOL` to compiled state `+0x356`. Constructor `0x35DAC..0x35DF0` scans those state bytes and caches whether any is set in live `+0xCD`. Damage routine `0x14F10` awards score after shields reach zero, then ordinary `+0xCD == 0` members enter `0x16300`; `+0xCD != 0` calls `0x17E70`, which scans states in file order and enters the first marked state through `0x146F0`, skipping ordinary destruction.

Canonical stock `Game.pak` contains 0 marked states among 1,167 states, so this executable-supported path requires synthetic compatibility coverage. The new `core_edge_runtime_test` raises the suite to 26/26 PASS. Canonical construction/first-tick outputs remain 386 groups / 546 members -> 544 active, with RNG seeds `2249411936` and `2633739833`.


## 2026-08-28 — Visual-state ramps and `0x12F20` render-request boundary

Re-entered PPC `0x12650`, `0x12750`, `0x12840`, `0x12940`, `0x12F20`, `0x12FA0`,
`0x13460`, state entry `0x146F0`, and the world draw loops around `0x34AC8/0x34B48`.
Entities and players share the first 0x94 bytes as a sprite/visual base. Face/frame,
scaled dimensions and half extents, geometry dirty, draw-to-terrain, draw layer, cached
sprite/frame handle, colorise, tint/visibility/scale current-target-delta triplets, and
collision-glow request fields are now offset-mapped. Live `+0x37/+0x38` are temporary
main/shadow pass selectors toggled by the world renderer, not persistent entity properties.

Parser/runtime correlation binds UnitDef initial scale/tolerance/visibility and draw-layer
fields plus state face/frame, parent-direction, tint/colorise/terrain-draw, required
scale/visibility/tint and deltas. `0x12750` and `0x12840` are reproduced exactly as scalar
ramps; visibility/tint clamp the decreasing side at zero while scale does not. Actual scale
movement dirties geometry, and `0x12940` later refreshes sprite dimensions/half extents.
Initial scale tolerance consumes the shared PPC-compatible RNG in its original signed
inclusive range.

`0x12F20` is now bounded as visibility gate -> shadow request -> main request. `0x12FA0`
submits the ordinary base only when `stateDoColorise_BOOL` is clear, then independent tint
and collision-glow requests. `stateDrawToTerrain_BOOL` bypasses ordinary main-layer
selection. Re-reading the PPC layer literals corrected earlier working notes: the FourCCs
are `plwe` and `play`, mapping to main layers 9 and 10. Zero/`none` is normalized to
`defa`. Shadow builder `0x13460` uses a separate domain: default/grou ground layer 2,
`grhi` layer 4, and default air/recognized air-player-HUD layer 6.

The clean `render_runtime` emits ordered headless requests rather than fabricating the
legacy QuickDraw backend. Canonical `Game.pak` validates all newly compiled fields and
reports 17 scale-tolerance units, 62 colorise states, 2 terrain-draw states, 111 nonzero
tint states, 584 non-100 visibility states, and 506 non-100 scale states. The suite is
28/28 PASS and the established constructor/first-tick RNG oracle remains unchanged.


## 2026-08-28 — Sprite resource cache, GIF atlas grammar, and `0x12940` geometry

Traced the resource path under the recovered render boundary through PPC `0x18D20`,
`0x19530`, `0x19AD0`, `0x19C10`, `0x19CA0`, `0x19EE0`, and the alpha-plate
scanner `0x1F140/0x1F1C0/0x1F340/0x1F4E0/0x1F540/0x1F5B0`. Loaded sprite
groups are 16-byte records containing a runtime marker, FourCC, frame count, and
frame-pointer list. The loader publishes the group only after all frame objects
are built. `0x19AD0` clamps an over-high requested frame to frame zero; `0x19C10`
uses stored dimensions at scale 1.0 and otherwise PPC `fctiwz` truncation;
`0x19CA0` lazily loads an absent group and retries. State entry stores the resolved
frame pointer at live sprite-base `+0x50`.

The atlas grammar is now exact. The scanner operates on decoded 8-bit GIF palette
indices, takes alpha-plate byte 1 as the separator marker, discovers marker-bounded
row bands and full marker columns, and trims each candidate cell using that cell's
own top-left palette value. A dependency-free clean GIF87a/89a indexed decoder and
exact scanner reproduce stock variable-size frames rather than assuming a regular
grid. Canonical results are 124 alpha plates, 124 color plates, 123 existing
alpha/color pairs with equal dimensions, and 2,463 alpha frames; `PDLI` is the
stock alpha-only exception. `PL1B` yields 7 frames with a 53x43 first frame,
`EXLG` 12, `BOCR` 3, and `GLOW` 12.

The resource cache is now wired into `0x12940`: dirty geometry resolves current
face/frame, applies lazy/high-frame cache semantics and PPC-truncated scaling,
then writes width/height and signed trunc-toward-zero half extents. A `none` face
zeros only the half extents and leaves width/height stale, matching the binary.
Negative frame indices are safely rejected rather than reproducing the original
out-of-bounds legacy indexing. The new sprite-resource regression raises the suite
to 28/28 PASS; canonical constructor/first-tick counts and RNG seeds remain
unchanged.


### DR-EVID-005 / AIFC-IMA4 audio-resource pass — 2026-08-28

A user-supplied `DeimosRising_soundtrack.sit` was inventoried as StuffIt 5 with ten method-0 MP3 data forks. The Theme Song carries ID3 title `Music 3[mu03]`; decoded comparison independently ties the released Theme/Game-3 material to canonical `Music.pak` `mu03`, while Interface/Advertising contain the canonical `inmu`/`ammu` loop material. Exact track hashes/metadata are recorded as DR-EVID-005; audio bytes remain outside Git.

The clean core gained a dependency-free FORM/AIFC parser plus Apple/QuickTime IMA4 decoder. Canonical packet semantics are 34 bytes/channel -> 64 samples, low nibble first, with the packet predictor's low seven bits recovered by retaining the previous running predictor when the new header is within `0x7f` and the step index is unchanged. This reproduces independent FFmpeg PCM sample-for-sample.

Canonical corpus validation now covers all 96 mono 44.1-kHz Audio.pak resources (3,133,376 decoded frames) plus all three stereo Music.pak resources. `mu03` also exposed an authentic legacy quirk: its declared FORM size is 76 bytes shorter than the valid chunk stream, so the parser now accepts under-declared FORM sizes while still rejecting overrun. The repository suite advances to 29/29 PASS; existing constructor/first-tick RNG oracles remain unchanged.

## 2026-08-28 — 16-bit sprite frame surfaces and exact shadow transform

The renderer-resource boundary was pushed below the atlas/cache layer. PPC `0x1D780` was reconstructed as the 16-bit frame-object builder and `0x1EEC0` as its optional transparency-plane constructor. The paired color plate is packed to xRGB1555 and cropped by the already-recovered atlas rectangle; the transparent color key is the third 16-bit color-plate pixel. The alpha crop contributes an inverted 5-bit transparency weight (`0` opaque, `1..31` blend, `32` transparent), while fully transparent rows carry the downstream blitter's `1000` first-word skip sentinel. The plane is omitted only when the whole frame can use transparent-key fallback.

Canonical replay across all 123 existing alpha/color pairs constructs 2,460 normal frame surfaces containing 3,115,564 color words and 3,115,564 transparency words, with 6,341 row sentinels. The aggregate deterministic surface oracle is FNV64 `0x9f9dcfba05b5089c`. Stock alpha/color FourCC tags differ by case, so the pair identity check is intentionally ASCII case-folded.

PPC `0x13460` was also closed. `Game[gafl]` entries 48..51 are explicitly `Shadow_XOffset=-48`, `Shadow_YOffset=104`, `Shadow_GroundXOffset=-6`, and `Shadow_GroundYOffset=8`. Air shadows use `0.5 * entityScale`; ground shadows use entity scale. `adjustShadowLocForScaling_BOOL` changes only the air-offset basis. The global returned by `0x100A0` is functionally bounded as the horizontal view offset used by world-space rendering; the `0x13460` terrain-shadow submission instead uses its fixed `-32` X basis and the `0xFEC0` background Y origin. The visibility helper `0x10C20` produces the legacy 0..32 transparency value and `0x13460` clamps it to a minimum of 20.

Two dedicated regressions were added (`sprite_frame_bitmap_test`, `shadow_runtime_test`), advancing the suite to **31/31 PASS** while the canonical constructor/first-tick and audio oracles remain unchanged.
## 2026-08-28 — Software compositor, queue, and terrain-target backend

PPC `0x18A40`, `0x19570`, `0x1A450`, `0x1A650`, `0x18B20`, the scale-1 compositor families `0x1D9F0..0x1E770`, scaled paths `0x1A6F0/0x1AA90`, and their sampling helpers were traced as one coherent 16-bit software renderer. The request record is 76 bytes. Low mode bits are overall transparency `0x1`, shadow `0x2`, solid-color overlay `0x4`, and terrain target `0x8`; `+0x31` is the direct/immediate selector. Queue records are copied by layer, layers 0/1 are one-shot, and flush groups are 0..1, 2..5, and 6..15.

The compositor preserves the frame's xRGB1555 pixels and optional 0..32 transparency plane. Normal blending uses destination-weight integer arithmetic; overall transparency and tint/glow add the request amount to the pixel mask. Shadow mode darkens the existing destination instead of drawing source color, with partial coverage using the recovered literal `0.032f` in `trunc(base + 0.032f * mask^2)`. Scaled paths use nearest-neighbor integer-ratio sampling, PPC-truncated extents, and the original untruncated-float centering quirk. The dormant `COST` constant-color rectangle path is also preserved.

Executable diagnostic strings identify two default-enabled toggles: `Sprite FX Enabled/Disabled` and `Sprite Alpha Drawing Enabled/Disabled`. FX-off copies the request while forcing scale 1/effect 0; alpha-off ignores the secondary plane and uses transparent-key fallback. Main `stateDrawToTerrain` requests are now proven to use one-shot layer 1 while terrain shadows use layer 0, both targeting the terrain surface through flag `0x8`. The semantic visual layer was corrected to emit raw 0..32 visibility/tint/glow amounts: `0x10C20` maps integer-truncated visibility percentages, while tint coverage is reduced by current visibility before conversion.

A new `render_backend_test` advances the repository suite to **32/32 PASS**. The canonical probe runs six compositor variants across all 2,460 stock frame surfaces (14,760 render passes) and freezes aggregate FNV64 `0x32290b39b091e970`; the source-surface oracle remains `0x9f9dcfba05b5089c` and all gameplay/audio/RNG baselines remain unchanged.



## 2026-08-28 — semantic-to-raw render orchestration and horizontal view controller

The recovered visual/runtime and software-backend layers are now connected end-to-end. `0x12FA0` copies frame identity, clip, scale and the sprite-base `+0x35` immediate selector into the 76-byte request; ordinary world-space X is `trunc(worldX)-0x100A0`, HUD/non-world-space bypasses the view offset, and tint/glow RGB24 is packed to xRGB1555 before submission. Main terrain stamps were corrected from an earlier collapsed description: `0x12FA0` uses `trunc(worldX)+32` and `trunc(worldY)+0xFEC0`, layer 1 and flag 0x8, while `0x13460` terrain shadows remain a distinct layer-0 path using the recovered shadow transform and its -32 terrain basis. Sprite-base `+0x90` gates the persistent main-terrain stamp by strictly newer render sequence.

`0x100B0` is the exact controller for the integer returned by `0x100A0`: each call clears the direction latch, applies +/-1, clamps to [-32,31], and records -1/+1 only when the movement did not saturate at a hard edge. A new `render_orchestration_test` advances the repository suite to **33/33 PASS** and exercises semantic state -> raw request -> queue -> software compositor end-to-end. The external canonical resource/gameplay probe remains unchanged.

## 2026-08-28 — Persistent terrain surface, vertical camera runtime, and full-viewport copy

Re-entered the recovered Mac 1.0.6 PPC application directly around `0xFA10`,
`0xFA90`, `0xFBC0`, `0x10000`, `0x10120`, and `0x10220` to close the terrain
surface ambiguity left by the render-orchestration checkpoint. `0xFBC0` loads
the level `im16`, validates its dimensions, allocates/resizes the long-lived
background raster at `ReqDisplayDepth`, copies the image into it, and disposes
the temporary source. Canonical `Game[gafl]` 54/55/56 are label-verified as
`VisibleGameWidth=416`, `VisibleGameHeight=480`, and `ReqDisplayDepth=16`.

`0xFA90` initializes the source Rect to the bottom-most 416x480 crop with a
literal +32 source-X bias and seeds vertical progress to 481. `0xFA10` then
invokes `0x33090` 545 times, from the source bottom through source top-64. This
resolves the old "strip" ambiguity: those calls activate simulation/world rows;
they do not copy background pixels.

`0x10220` is a pure source-Rect/scroll-accounting step. It subtracts requested
vertical delta from top/bottom, adjusts/clamps progress, clamps the view to the
persistent surface, and publishes `oldTop-finalTop` as the applied delta exposed
by `0xFED0`. `0x10000` adds the end latch and one new `sourceTop-64` row
activation. Because progress starts at 481, ordinary +1 scrolling reaches the
full-height end condition with source top still at 1; the clean regression
preserves this one-pixel quirk.

Most importantly, direct `0x10120` disassembly disproves the prior incremental
strip-copy/dirty-region hypothesis for core gameplay. It clones the current
source view, changes left to `max(horizontalOffset+32,0)`, sets right to
`left+416`, builds destination `{0,0,480,416}`, and calls the existing surface
copy helper from the persistent terrain raster to the visible/main surface.
The game therefore copies the full 416x480 terrain viewport on every call.
Persistent terrain stamps survive because layer-0/1 writes mutate the long-lived
full raster itself.

The surrounding `0x30BC0` renderer now also gives a direct next-stage ordering
oracle: `0x18B20(group0)` terrain writes -> `0x10120` full viewport copy ->
`0x18B20(group1)` -> `0x43BA0` -> `0x18B20(group2)`. This ordering is documented
but intentionally left for the next clean frame-orchestration checkpoint.

Added `LegacyTerrainSurfaceConfig`, `LegacyTerrainSurfaceRuntime`, exact prime /
step / tick / viewport-copy helpers, and a dedicated terrain surface regression.
The suite is now **34/34 PASS in Debug**. The real canonical `Game.pak` probe
reports `terrain viewport/depth: 416x480x16` while retaining 386 groups / 546
constructed members / 544 first-tick active members and RNG seeds `2249411936`
/ `2633739833`.

## 2026-08-28 — Exact particle raster and outer world-frame composition

Direct PPC re-entry around the previously unresolved `0x43BA0` slot corrected an
important architectural assumption. `0x43BA0` is not presentation: it is the
particle subsystem's direct xRGB1555 rasterizer. The surrounding executable
cluster is coherent: `0x43340` constructs particle systems, `0x438C0` updates and
prunes them, `0x43BA0` draws them, and `0x44550` clears the subsystem. Canonical
`Game[gafl]` indices 144..148 are label-bound as `Particle_Gravity=0.96`,
`Particle_ColorVariationAdjust=0.12`, `Particle_FringeColorAdjust=0.6`,
`Particle_BlendAmountRate_Short=3.0`, and `Particle_BlendAmountRate_Long=1.0`.

The `0x43BA0` unrolled 7x7 kernel was symbolically reconstructed exactly. It
subtracts the `0x100A0` horizontal view offset, performs strict float clipping
against `0.0` and `+7.0` before PPC truncation, and blends fringe/core xRGB1555
colors through radial transparency bands `min(q+22,31)`, `min(q+10,31)`,
`min(q+6,31)`, and `q`. The five core-color taps form a plus. The center carries
a genuine legacy discontinuity, using `q>6 ? q-7 : q`; the regression freezes
that behavior rather than regularizing it.

Direct caller disassembly also closes the outer composition segment at
`0x30BC0`: `0x18B20(group0)` -> `0x10120` full terrain viewport copy ->
`0x18B20(group1)` -> `0x43BA0` particles -> `0x18B20(group2)`. The caller's r28
draw latch gates only the terrain copy and particle pass, not the three queue
flushes. `LegacyParticle`/`LegacyParticleSystem`, `rasterize_legacy_particles()`,
and `render_legacy_world_frame()` now encode these facts directly. Dedicated
particle and world-frame regressions raise the repository suite to **36/36 PASS
in Debug**. The canonical `Game.pak` probe label-validates particle tuning while
preserving all prior gameplay/RNG/render hashes and the 416x480x16 terrain
contract.


## 2026-08-28 — Particle construction/update and gameplay producers

Disassembly of `0x44630/0x431F0` proves startup constructs 100 normalized random directions and parallel speed-varied copies (1/.85/.70/.55), then seeds two 0..99 cursors. `0x43340` recognizes exactly eight preset FourCCs (`tiny/smal/med /larg` plus `tici/smci/meci/laci`) for 5/10/20/40 particles and 3/5 velocity magnitude. Unknown IDs return without RNG. `0x438C0` applies the `0xFED0` terrain delta only to ground-space systems, damps both axes by `Particle_Gravity`, integrates, applies x>=-32 / x+7<=width+32 / y>=0 / y+7<=height, and uses the long blend rate for the recovered forward/reverse lifetime branches.

Three direct `0x43340` callers are now bound. `0x33A7C..0x33B60` uses state fields +0x2D0..+0x2DC and live +0xF0/+0xF4; state entry clears only +0xF4. `0x150BC..0x15114` emits non-lethal hit particles from UnitDef hit fields. `0x1636C..0x163C4` emits destruction particles before later destruction consequences. Canonical source validation corrects an earlier temporary-corpus assumption: 7 state-particle states exist and 4 have repeat enabled; 3 hit-particle units exist and all have the source circular Boolean false. Suite: 37/37 Debug PASS; canonical gameplay/render hashes and RNG seeds unchanged.


## 2026-08-28 — Native QuickDraw presentation boundary

Re-entered `0x30BC0` after the closed particle/world compositor. The routine has a second independent gate at `0x30D8C`: if enabled, frame-object byte +0x04 dispatches mode 0 to `0xBC60` and mode 1 to `0xBEB0`. The normal game construction path (`0x56AC -> 0x30210`) stores mode 1, while a non-gameplay path at `0x2E554` stores mode 0.

The display manager (`0xAE20..0xB51C`) binds `Game[gafl]` 52..60 to a centered 640x480x16 frame: 32 left border + 416 game + 160 score bar + 32 right border. `0xBC60` performs one 640x480 QuickDraw `CopyBits`; `0xBEB0` copies source game `{0,0,480,416}` and source score bar `{0,416,480,576}` into their centered destinations, with black `PaintRect` side strips when the host display is wider than 640. Imports and helper calls identify `SetGWorld`, `SetRect`, `PaintRect`, `ForeColor`, and `CopyBits`.

A deeper pass classifies `0x9E40` as only a GWorld activation/`SetGWorld` helper, correcting the temporary hypothesis that it might draw UI. The score-bar pixels must already exist before `0xBEB0`, making their producer path a separate next target. Added `LegacyPresentationConfig`, plan/execution helpers, and `presentation_runtime_test`; Debug suite 38/38 PASS and canonical `Game.pak` adds the 640x480x16 layout oracle with prior hashes/seeds unchanged.

## 2026-08-29 — Score-bar producer/cache and score threshold runtime

Traced the 160-pixel score-bar producer cluster immediately following the closed
`0x30BC0` world/presentation path. `0x30F40/0x31400/0x317E0/0x31AE0` form a
332-byte-per-player cache with six dirty classes: score, life symbol, life
count, three weapon previews as one class, shield, and power. `Game[gafl]`
111..143 and `Rects[inre]` 0..15 now form a label-verified layout/rate contract.
Shield converges at +2/-3, weapon power at +2/-4 only increasing in active
status 4; relocated PEF data proves the final power clamp constants are exactly
0.0 and 100.0. `0x32050` displays `clamp(lives-1,0,9)`.

The upstream score getter/setter pair `0x299F0/0x29A00` and award routine
`0x29A10` are now represented in `PlayerRuntimeSlot`. Normal awards multiply by
the live bonus multiplier, use a strict `newScore > threshold` life test, and
consume at most one threshold per award. Canonical life score fields are
10000/30000 with `Player_ExtraLifeScoreAdjustment=10000`. Player score-bar
sprite resources and all five canonical weapon preview descriptors are also
compiled. Suite raised to 39 tests; canonical sprite/software-render hashes and
construction/motion RNG seeds remain unchanged.


## 2026-08-29 — score-bar and display-commit closure

- Traced `0x30F40..0x32A70` as the gameplay score-bar object/update/draw cluster feeding source canvas x=416..575.
- Label-verified Game[gafl] 111..143 and Rects[inre] 0..15, including six dirty classes, 2/3 shield rates, 2/4 power rates, 0.7 nonselected weapon scale, and max displayed lives 9.
- Recovered `0x299F0/0x29A00/0x29A10` score production and extra-life thresholds; canonical PlayerDefs use 10000 initial / 30000 additional and Game[gafl] 182 contributes 10000 adjustment.
- Confirmed all five canonical weapons expose score-bar preview face/frame descriptors.
- Mapped PEF DrawSprocket imports: GetFrontBuffer exists; GetBackBuffer and SwapBuffers do not. `0xC81C` is the only GetFrontBuffer call and feeds `0x44B50`, a four-bound extractor only.
- Traced `0xAE20` -> `0xA640` (`NewCWindow`) and `0xC2A0` -> `0xA980` (`SetGWorld`) -> `0xAC20` (`CopyBits`), proving the final legacy commit is immediate QuickDraw drawing into the CWindow port with no explicit DrawSprocket swap after the presenter.


## 2026-08-29 — score-bar pixels and complete visible-frame order

- Recovered original gameplay-loop ordering: `0x5A18 -> 0x7070 -> 0x31AE0` score-bar draw precedes `0x5AB0 -> 0x30570 -> 0x30BC0` world composition/presentation.
- Located the small score-bar font in sibling `Interface.pak`, not `Game.pak`: `Text - Small IA[TESM].gif` / `IC[tesm].gif`, 852x18 plates, exactly 91 reconstructed frames.
- Bound score formatting `%0.7i`, life formatting `%i`, canonical cyan color `#94DEE6`, and dedicated red `#FF0000` final-life count.
- Added clean 16-bit TGA decode for canonical `Scorebar[scor].TGA` (160x480), exact dirty-background restoration, text/sprite/meter pixel paths, and canonical score-bar sample FNV64 `0xd2f48984985f54d8`.
- Audited the remaining nearby `COST` overlay and identified it as level-selection acceptance/failure scaling (`LevSel_Acceptance_*` / `LevSel_Failure_*`), not a missing gameplay HUD path.
- Added `render_legacy_gameplay_frame()` to bind score-bar pixels, 0x30BC0 world order, 576x480 source composition, and mode-1 native presentation under one tested clean-core boundary.


## 2026-08-29 — level-selection acceptance/failure `COST` pulse

- `Formats[gate].idli` proves runtime ordinal 27=`lsca`, 28=`lscf`, matching the two style fetches in `0x2FE40`.
- Canonical `lsca` is green `#00ff00`, blend 16; `lscf` is red `#ff0000`, blend 16.
- `0x2FCC0` selects Game[gafl] 44/45 for acceptance (`0.18`, `2.0`) and 46/47 for failure (`0.25`, `2.0`). Blend rises one unit/tick toward 32 while scale ping-pongs 0→max→0; canonical teardown is 24 ticks versus 16 ticks.
- `0x2FB88..0x2FC14` submits the result through the already-recovered `COST` solid-rectangle compositor path. This confirms the effect is a front-end level-select pulse and does not reopen the closed normal-gameplay frame chain.

## 2026-08-29 — original-data live frame oracle

- User-validated native macOS Metal screenshot closes the complete original-data frame path for `Kepler Massif [le01]` / Player 1.
- Complete canonical display FNV64: initial `0x9e8a7ec73b79b254`; after one recovered terrain/HUD tick `0x44dede08075273f2`; after 30 ticks `0x51d4a7eec9b0beef`.
- `Game[gafl]` first labeled value is `FPS_MaxRate=30`; the Apple live integration cadence now consumes this source value.
- Persistent frame state now includes terrain camera/source view, score-bar cache, game/source/display surfaces and tick/render counters.


## 2026-08-29 — placement-row scheduling, live rule facts, and removal host closure

Re-entered the terrain/world activation seam after the first live-session overcrowding regression. Terrain bootstrap `0xFA10` calls world routine `0x33090` for each row through `sourceTop-64`; normal scrolling calls it for the single newly exposed row. Canonical Level 1 starts at source Y 3120..3600, so the initial 64-pixel margin reaches 3056 and releases only serialized placements Y 3309 and 3129. The next Y 3020 placement activates on tick 36. Added `LevelPlacementActivationRuntime`; new-row entities are constructed against the already-advanced world origin and excluded from that tick's camera shift.

Audited state-rule execution and found the recovered five-rule evaluator was receiving no live `facts_for_rule` provider. Canonical corpus enumeration shows 5,835 slots and, critically, all 2,773 `Is Tracking Player` slots reference sentinel Unit ID `none`; substituting the later member target flag would therefore have manufactured widespread Delete transitions. Added `UnitRuleWorldRuntime` for Unit-ID/range active/tracking queries, active counts, player/global destroyable-domain facts, and visual scalar facts while keeping `Animation Stopped` and the exact lower-level range helper as explicit boundaries.

The collision host also omitted the recovered removal transaction: scanners received no `LegacyRemovalContext`, so lethal damage could skip immediate `0x16300` consequences, and `0x36610` was never called afterward. The live tick now supplies canonical random-bonus/water-impact configs, finalizes inactive members once after collision traversal, persists random-bonus/ground-obstacle state, constructs consequence spawns after traversal, and re-syncs visuals after collision-driven state transitions. Persistent obstacle Rects now scroll with terrain and use the already-recovered ground-member stop/latch helper. Level Media Mask sampling and visible particle-system execution remain unwired rather than guessed.

Canonical external-data witness after the correction: 2 initial placements/members; third placement tick 36; 3 placements by tick 120; 24 allocated members; max 20 active; 7 finalized removals; 10 removal consequences; 1 consequence spawn; 0 entity/entity collisions in the current fire/no-aim soak. Static, tick-1, tick-30, right-input, live-initial and live-fire hashes remain unchanged; scheduler-corrected tick-120 live hash is `0x13c37d4b847666f9`. Synthetic suite: 53/53 PASS.


## 2026-08-30 — Media Mask host binding and collision-pair aggregate closure

Closed two previously explicit live-host plumbing gaps without guessing their remaining higher-level orchestration. `CollisionScanResult` now retains each successful `CollisionPairResult` in exact traversal order, so `0x14F10` collision-spawn facts are no longer discarded by `0x36CF0` aggregation. The live tick exposes a collision-spawn-due count but does not yet construct those objects; original spawn position/ownership semantics remain a separate PPC closure target.

Bound the level's decoded original 16-bit Media Mask into `LegacyRemovalContext::water_probe`. The host derives world-to-cell scale from the serialized `LevelDefinition::background` rectangle and actual decoded mask dimensions, rejecting non-integral pairings rather than hard-coding Level 1. Canonical `le01` pairs 480x3600 with `cat1` 96x720, giving exact 5x5 cells. Corpus inspection of `Canyon 1 Media[cat1].TGA` finds only normalized xRGB1555 values 32767 (65,206 cells) and 31 (3,914 cells); `0xFEE0`'s recovered value-31 water classification therefore has a concrete provider. Exact internal `0xFEE0` address arithmetic is still tracked separately for instruction-level closure.

Synthetic Debug suite remains 53/53 PASS. The external canonical frame/live probe also remains unchanged: static/tick/control/live frame hashes all match, Level 1 still activates 2 initial placements and the third at tick 36, and the 120-tick witness remains 24 allocated / max 20 active / 7 removals / 10 consequences / 1 consequence spawn / 0 entity collisions / 0 collision-spawn requests, with tick-120 FNV64 `0x13c37d4b847666f9`. This is strong negative-regression evidence that enabling real water classification did not perturb unrelated scheduling/combat behavior.


## 2026-08-30 — Mac playable wrapper failure reproduced and HUD producer bridge

The core was exercised directly from the canonical PAKs after a native-device report of “no enemies / cannot shoot.” `enable_live_world()` contains two opening `bsde` members at screen coordinates `(82,189)` and `(229,9)`. A first-tick Ion Cannon press constructs five members total from the three serialized weapon requests; after several ticks the two `icb ` bolts are visibly north of Player 1. This isolates the report from entity construction and into the native wrapper/input boundary.

The previous macOS smoke wrapper allowed live bootstrap failure to degrade to bounded Preview mode, which is visually deceptive because Preview still renders the real terrain/HUD. The wrapper is now fail-fast and uses direct `NSWindow` responder key delivery. The live score-bar weapon producer was separately corrected to respect Weapon Definition level-availability rather than copying the first three corpus weapons. Locked slots are represented by absent descriptors after static-panel restoration.

Damage facts now retain source player ownership through aggregate collision traversal, allowing the live owner to consume `CollisionDamageResult::score_award` through recovered player-score semantics. No canonical 120-tick no-aim collision occurs, so this bridge does not perturb the scheduling/removal witness. The live frame witnesses changed only because the Level-1 weapon HUD is now correct: `0x1eb1e07d4b6d038d`, `0x1e24b6143cd762ec`, `0x13c37d4b847666f9`. Static frame witnesses remain unchanged. Synthetic 53/53 PASS; canonical Game.pak and full-frame probes PASS.

### 2026-08-30 — native playtest: slowdown / crash effects / secondary fire / HUD

A native Mac playtest reported progressive slowdown, correct enemy projectile destruction but absent player-crash explosion animation, missing-feeling secondary fire, sparse HUD feedback, working coin appearance, and correctly placed ground sprites. Reproduction separated these into host-orchestration failures rather than placement regressions.

The slowdown had two independent causes. First, the clean vector world retained every finalized inactive member and every visual record indefinitely; repeated `find_member` / visual searches therefore grew with historical allocation. `prune_finalized_history()` now physically removes only records whose recovered removal transaction has completed. Second, a 1800-tick stress showed 203 *active* resident objects, including Ion/projectile families thousands of pixels above the view and enemies/effects thousands below it. Because the exact PPC outer live-list cull caller is not yet mapped, the playable host now uses a conservative one-full-viewport guard outside the visible rectangle before normal deletion. A 3000-tick stress changed from max/final ~220/219 and ~14.5 s to max/final 91/14 and ~3.6 s in the current headless environment.

The crash-animation failure was direct evidence of dropped recovered outputs: the collision callback called `apply_legacy_player_damage()` and discarded its `LegacyPlayerDamageResult`. Player 1 canonical data uses `active_SpawnOnHit=plsh` and `death_Spawn=plde`. Those object consequences, shield warning, death-money drops, pickup/life spawn and lifecycle entry spawn are now deferred through normal `SpawnRequestSeed` construction after stable collision traversal. The live host also executes `advance_legacy_player_lifecycle()`. A deterministic opening-lane ram reaches dying status at tick 185 and respawns at tick 266 with lives 2 and shield 100.

The already-recovered particle subsystem was also not attached to the live host. State and removal contexts now carry `LegacyParticleExecutionContext`, live particle systems update each game tick and are passed to the gameplay-frame renderer. Direction-table startup currently uses a copy of the deterministic RNG stream because the exact global 302-draw startup ordering relative to level construction is not caller-closed; actual gameplay particle producers still consume the shared entity RNG inline.

Ground/secondary fire was verified independently from the canonical PAK: the first semantic ground-fire tick launches and constructs `plbo`, `pblf` and the initial bomb-glow/effect chain. macOS adds Shift as a secondary-fire alias to X. Live HUD power no longer invents a full 100% target; the power-up producer remains a future recovery item, while score/lives/shield and available-air-weapon preview data are real live Player-1 state.


## 2026-08-30 — Real 1.0.6 PEF recovered; PPC Lab closes charge, lifetime cull, and collision-spawn request

Recovered the original application executable from preserved installer evidence rather than relying on the prior static inventory: StuffIt 5 archive -> 81,788,928-byte HFS self-mounting image -> `Deimos Rising` APPL data fork. The extracted PowerPC PEF is 2,045,976 bytes with SHA-256 `8e436c3babc582f1407ae6fed47e9749f1c930335ce4c794947e40b06b85eb29`. The executable bytes remain external evidence and are not added to the repository. PPC Lab recognizes the PEF directly; the main transition vector yields TOC `0x100e6330`, enabling deterministic internal routine calls against shipped code.

**Weapon charge `0x3B3C0`.** The handler reads `TimeUntilActivation +0x1E8`, activation ID `+0x1EC`, power interval `+0x1F0`, max level `+0x1F4`, release ID `+0x1FC`, release cadence `+0x200`, and `DoReleaseOnMaxPowerLevel +0x204`. Release processing emits one release spawner per attained integer power level and decrements the level once per emitted spawner. A direct PPC Lab call with serialized `OverloadTime +0x1F8 = 1`, a long-expired hold, max power, and `DoReleaseOnMaxPowerLevel=false` returns still charged at level 20 / 100%; the WIP4 host's overload-time auto-release was therefore fabricated and has been removed.

**Main member lifetime `0x12CA0`.** Main tick calls it with margin 128 and immediately marks the member deleted when false, before later member processing. PPC Lab boundary sweeps with viewport 416x480 and half extents 10x10 prove: x=-139 rejects / -138 survives; x=554 survives / 555 rejects; y=-129 rejects / -128 survives; y=618 survives / 619 rejects. This closes the post-movement predicate as `x+halfWidth >= -128`, `x-halfWidth <= width+128`, `y >= -128`, `y-halfHeight <= height+128`, with equality surviving. The previous one-viewport late host bound has been replaced with this exact early gate.

**Collision spawn `0x14F10`, block `0x1516C..0x1525C`.** The routine copies the global 44-byte default constructor request then stamps the pre-hit state's collision Spawn ID, damaged target x/y, damaged target owner, and damaged target pointer/serial as parent before calling `0x33220`. PPC Lab dump of the default template confirms owner=-1, flags/headings clear and velocity multiplier 1.0. `CollisionDamageResult` now carries the complete request; live-world orchestration constructs these requests after stable collision traversal while preserving first-leg/second-leg order.

Regression after translation: synthetic suite 53/53 PASS; canonical Game.pak validation PASS; historical static/tick/right-control frame hashes unchanged. Tick-120 live hash remains numerically `0x055b51228f651199`; exact early culling lowers the 120-tick peak active count from 20 to 19. `deimos_playable_runtime_probe` still proves crash dying@185 / respawn@266 / lives=2, Plasma Bomb secondary fire, 15-tick charge activation and `icps` release. The 3000-tick stress is bounded at maxResident=114, finalResident=10, pruned=1862, exact farCulled=236, approximately 3.17 s on the current headless host.

## 2026-08-30 — Shared 0x12BC0/0x12C10 hit/pickup glow restored; player-impact damage results consumed

The shipped executable exposes a shared sprite-base feedback helper at `0x12BC0`, with updater `0x12C10`. The trigger stores active=1, amount=32, direction=toward-peak, rate from r5, and packed xRGB1555 color from r4. Ordinary collision damage calls it as `(target, 0x7FFF, 6, 0)` when the pre-hit state allows collision glow; coin pickup uses the same white/rate-6/no-restart call on the player. The updater subtracts rate until amount <= 4, clamps/toggles, then adds rate until amount >= 32, clamps/toggles and disables. Canonical rate 6 therefore yields 32 -> 26 -> 20 -> 14 -> 8 -> 4 -> 10 -> 16 -> 22 -> 28 -> 32/off.

`LegacySpriteVisualRuntime` already had the renderer-facing collision-glow pass but lacked the trigger/updater state. The clean runtime now mirrors the pulse fields and advances them once per game tick before later collision triggers. Entity/entity collision damage triggers the target visual; accepted coin pickup triggers Player 1. Existing active pulses ignore r6=0 retriggers, matching the shipped early return.

Review also found that the live host discarded `PlayerCollisionScanResult` after entity-vs-player impact. That result contains the same `CollisionDamageResult` produced by `0x14F10`, so player rams could still lose entity hit glow, collision-spawn construction and player-attributed kill score. The host now consumes all three after each player collision event while preserving reciprocal player damage/death ordering. Synthetic 53/53 PASS; original frame oracle remains byte-identical; playable crash/secondary/charge/stress gate remains PASS.


## 2026-08-30 — WIP6 secondary-input, owner-death cleanup, and ground-reticle closure

Device playtesting exposed two separate host defects behind the apparent missing secondary fire and persistent GET READY message. On AppKit, left/right Shift are modifier transitions delivered through `flagsChanged:` rather than ordinary key-down/up events; the native host now maps those transitions to semantic ground fire while retaining X. The external playable probe was strengthened from a launch-only witness to an actual Level-1 damage oracle: one canonical Plasma Bomb now must reduce the opening left `bsde` ground emplacement from shields 4.0 to 3.6.

The persistent GET READY visual was not the respawn entry effect. `nosw` (Shield Warning) requests `noti` frame 4, but the portable state-visual bridge was incorrectly entering every state at frame 0; `noti` frame 0 is GET READY. State entry now begins at serialized `stateSpriteFrameMin`. PPC also closes the missing death ownership cascade: player-death helper `0x27E50` calls `0x34B90(playerIndex)` before death consequences; that routine walks player-owned members and applies the current state's destroy-on-owner-destruction/delete-on-owner-deletion policy. The clean host now mirrors that transaction. Framebuffer dumps at ticks 266, 270, 276, 282, 300, 311, 312 and 320 confirm no GET READY text survives the respawn sequence.

The shipped Plasma Bomb targeting reticle is now restored. Weapon runtime `0x3B3C0` copies selected-ground Weapon Definition `+0x168/+0x16C` into a persistent sprite and `0x3BAB0(controller,1)` selects the locked frame when target scanning succeeds. Player update `0x3BB00` anchors the weapon controller at player X/Y; ground launch `0x3C4F0` closes the serialized offset sign, yielding `crosshair=(playerX+XOffset, playerY+YOffset)`. The sprite base constructor uses layer `defa`. Canonical `plbo` therefore renders `pbta` frame 0 at offset `(0,-121)` and flips to frame 1 when its AABB overlaps an eligible hittable ground target. The playable probe now freezes normal frame, locked frame, exact offset, launch and actual ground damage end-to-end.

Static/non-live frame witnesses remain unchanged. Because `pbta` is newly visible only in live mode, clean live integration hashes intentionally become initial `0xbdf7558de9357ff7`, first-air-fire `0x036bb03279ae5b48`, and tick-120 `0x8e4063956c4df5cc`; world counters remain 16 resident / 10 groups / max 19 active at tick 120. Synthetic suite remains 53/53 PASS.

## 2026-08-30 — Original resource-fork front-end recovery

Re-extracted the 1.0.6 application resource fork from the preserved StuffIt/HFS installer chain and inventoried its front-end resources. `MBAR 128`, `MENU 128/2000/2001`, `DLOG/DITL 190..193`, `STR# 130`, and `tset 493/2558` prove the shipped About/File/Edit menus, Preferences/Controls/HID dialogs, key-name table, and default keyboard set. DITL 190 names Full Screen, Interlacing, Bypass System Volume, Music/Sound Volume, ESC Key Delay, Set Controls and Set Gamepad Controls. The first seven default keyboard codes are arrows plus Option/Command/Space; DITL 191 labels the final three actions Air/Ground/Select. The macOS host now presents launch/pause/control/preferences menus and accepts those original Player-1 defaults alongside modern aliases. Original application/resource bytes remain outside the repository.

## 2026-08-30 — WIP8 shared animation/orientation and main-tick ordering pass

Reconstructed the missing live sprite-animation/orientation layer from the `0x146F0`, `0x15930`, `0x16230`, and `0x172D0` witnesses. Compiled state data now supplies direction count, frame bounds, frames-per-direction, delay/delta, backward/loop/randomize, RotateToTarget, parent-direction, and vertical-scroll-pause flags. The live member carries frame/direction/cadence/stopped state; animation executes between timer and rules, making finite-animation stop visible to the rule evaluator in the same tick. RotateToTarget was kept visual-only and its sign/wrap behavior was regression-tested instead of steering physical heading.

Re-entered the WIP7 host ordering against the `0x33850` main-loop witnesses. Screen movement/lifetime is no longer performed before rule/target/motion work. Owner modes remain before `0x15B40` spawn scheduling, and `0x344F8..0x34578` obstacle stopping now occurs after due spawn requests are built so terrain-effect eligibility sees the pre-stop stationary state. The `0x33A54..0x33A78` delayed-member gate now lets a 1 -> 0 member run immediately. `statePauseVerticalScrolling_BOOL` now feeds a following-frame outer terrain-scroll latch.

A long Level-1 audit exercised all 36 Flipper Mk2 and Screw Mk3 directional frames and reproduced the scripted multi-state scroll holds around the same tick regions found in PPC-Lab research. Investigation of apparently extreme Flipper frames separated the animated `raso` south-flight state from the directional Hunt state; Hunt frames agreed with player target geometry. The ground-position audit retained the already-closed absolute/relative spawn transform and rejected a global coordinate shift.

The playable crash timing moved to 171/252. Isolation proved that WIP8 ordering with animation disabled is 184/265 and restoring the old delayed-member gate on top of that is exactly 185/266, while full animation/orientation + stopped-rule behavior supplies the larger trajectory shift. Static frame hashes did not move; only live-world hashes were re-frozen after the cause was understood.
