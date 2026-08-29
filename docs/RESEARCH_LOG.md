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
