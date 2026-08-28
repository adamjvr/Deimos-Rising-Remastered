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
