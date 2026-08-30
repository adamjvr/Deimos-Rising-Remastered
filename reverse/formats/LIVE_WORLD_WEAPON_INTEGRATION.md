# Live world + player weapon integration

Status: **current 2026-08-29 live-host contract; 53/53 synthetic tests PASS plus canonical external-data probe PASS**.

This milestone upgrades the external-original-data preview from a static Player-1
fixture into a persistent clean world while preserving an explicit distinction
between instruction-closed runtime behavior and still-bounded host orchestration.

## Level placement activation is now scheduled

`enable_live_world()` no longer constructs every serialized Level-1 placement.
The recovered terrain bootstrap at PPC `0xFA10` calls world routine `0x33090`
one world-Y row at a time from the initial source bottom through
`sourceTop - 64`; normal scrolling calls the same world routine for exactly the
new `sourceTop - 64` row.

`LevelPlacementActivationRuntime` mirrors that one-shot row stream and releases
placements in serialized source order. For canonical `le01` / `cam1`:

- initial terrain source view: Y `3120..3600`;
- initial activation margin reaches world Y `3056`;
- Level-1 placement Y values range from `3309` down to `204`;
- only the placements at Y `3309` and `3129` are reached during bootstrap;
- the third placement (`fl02`, Y `3020`) activates at live tick **36**.

This corrects the earlier WIP behavior that expanded the whole placement corpus
into 151 live members before the player had scrolled to those encounters. New
members created by the just-activated row already subtract the new world-Y
origin and therefore are not camera-shifted a second time.

The row-to-placement binding is strongly corroborated by the binary call shape
and canonical data, but the internal implementation of PPC `0x33090` remains a
separate instruction-closure target. That uncertainty is isolated in the
scheduler boundary instead of being hidden in the game loop.

## Player authority and weapons

`PlayerWorld::slots()[0]` is authoritative for live Player 1. Host movement,
weapon launch geometry, collision/pickup mutation, score-bar state, and render
coordinates all consume that same object during a tick; `player_runtime_` is a
public/static-preview mirror at the live boundary.

`live_player_weapon_runtime` compiles all five original Weapon Definitions and
selects the `DEAA` / `DEAG` defaults for the current level. Level 1 defaults to
the Ion Cannon (`aiic`), whose canonical launch emits the three serialized
constructor requests:

- `icb ` at Player X-5 / Y+0;
- `icbf` at Player X+0 / Y-8;
- `icb ` at Player X+4 / Y+0.

Ownership, non-auto-repeat edge behavior, launch delay, and level-availability
switching remain covered independently of unresolved original film/input bits.

## World-dependent state rules are live

The recovered `0x15550` five-rule evaluator was already present in the clean
entity runtime, but the first live host failed to supply `facts_for_rule`; that
made every rule slot inert. `UnitRuleWorldRuntime` now supplies the original-
shaped facts at the correct pre-target-dispatch stage:

- Unit-ID/range active query;
- Unit-ID/range tracking query;
- active count for a Unit ID;
- players-active and closest-player range facts;
- no-destroyable-air / no-destroyable-ground global facts;
- current/required visibility, tint, and scale facts.

A canonical corpus audit is important here: all **2,773** `Is Tracking Player`
rule slots in stock Mac 1.0.6 reference sentinel Unit ID `none`. They are
therefore inert/default slots and must not be replaced with the current
entity's later `has_active_target` flag. The live bridge preserves the Unit-ID
query shape, preventing the catastrophic delete transitions that such a
shortcut would create.

The exact lower-level spatial helper behind nonzero range queries remains an
evidence-isolated clean implementation pending its own PPC closure. Stock
`Is Tracking Player` slots do not exercise a non-sentinel target. The
`Animation Stopped` world fact is also intentionally left false until sprite
animation timing is instruction-closed.

## Destruction and removal are connected to collisions

The collision core already reconstructed lethal `0x14F10 -> 0x16300` behavior,
but the first live host passed no `LegacyRemovalContext` and never executed the
outer inactive-member pass at `0x36610`. A lethal hit could therefore merely
flip lifecycle state: the object vanished from rendering while destruction
spawns, group counts, deletion spawns, random bonuses/coins, owner-child
propagation, and terrain-obstacle consequences were never committed.

`tick_live()` now owns the recovered removal transaction:

1. build one `LegacyRemovalContext` for the current tick using canonical random-bonus and water-impact tables;
2. pass it through entity/entity and entity/player collision scans so immediate `0x16300` consequences keep shared-RNG order;
3. run `finalize_legacy_pending_removals()` once after collision traversal, preserving `0x36610` forward-pass behavior;
4. gather emitted `SpawnRequestSeed`s and construct them only after traversal so stable vector references are not invalidated;
5. synchronize surviving visuals if collision damage entered an on-hit/shield-depletion state;
6. persist random-bonus context and the recovered destruct-to-terrain obstacle Rect list across ticks.

The ground-obstacle list receives the same vertical screen delta as live members
(PPC `0x2A770`), and the recovered `0x344F8..0x34578` ground-member stop/latch
query is executed after motion. Actual level Media Mask sampling is **not** yet
wired: absent a `water_probe`, the clean `0x16880` resolver deliberately
preserves the requested ordinary spawn instead of guessing that a point is
water. Destruction particle consequence facts are emitted, but visible particle
systems are not yet attached to the live renderer.

## Current 30 Hz tick sequence

The bounded live session now performs:

1. apply semantic host movement to authoritative Player 1;
2. advance terrain and activate only the newly reached placement row;
3. shift existing live members and persistent obstacle Rects by camera delta;
4. launch/construct canonical player weapon requests;
5. advance active entity screen motion and ground-obstacle stop behavior;
6. evaluate state rules from live world facts, then target/owner/motion/spawn scheduling;
7. construct due state-spawn requests after entity traversal;
8. run entity/entity and entity/player collision with immediate destruction effects;
9. finalize pending group/member removals and construct consequence spawns;
10. update Player-1 mirror/HUD caches and render active live members.

## Canonical regression witness

The original-data probe keeps all pre-live presentation oracles unchanged:

- static initial frame: `0x9e8a7ec73b79b254`;
- no-input tick 1: `0x44dede08075273f2`;
- no-input tick 30: `0x51d4a7eec9b0beef`;
- one-right-input tick 1: Player `(209.6,330)`, velocity `(1.6,0)`, frame `0x6fd5c94a64dcb0c8`.

Live-world integration witnesses are separate clean-runtime regressions, not
claims of original executable screenshot capture:

- initial members / active / placements: `2 / 2 / 2`;
- first later placement activation: tick `36`;
- placements activated by tick 120: `3`;
- allocated members at tick 120: `24`;
- maximum active through tick 120: `20`;
- entity/entity collisions in this particular no-aim soak: `0`;
- finalized removals: `7`;
- removal consequences: `10`;
- consequence spawn requests: `1`;
- live initial frame: `0x864f27d9c3820d7f`;
- first tick with air-fire held: `0xe94cfb91faa42e72`;
- tick-120 frame: `0x8a8f770b4de4d2cb`.

`deimos_original_frame_probe /path/to/Paks` now hard-fails if those deterministic
scheduler/removal counters or frame witnesses drift.

## Host controls

The macOS integration host currently uses modern semantic bindings:

- arrows or WASD: movement;
- Space: selected air weapon;
- X: selected ground weapon;
- Tab: cycle level-available air weapons.

These do **not** claim bit assignments for the unresolved original
InputSprocket/film action mask.

## Remaining boundaries

The current live host still does not claim closure of:

- exact internal PPC `0x33090` placement-row matching implementation;
- exact low-level spatial helper used by nonzero Unit-ID/range rule queries;
- `Animation Stopped` rule timing;
- actual Level Media Mask decode/sampling for water-impact replacement;
- visible live particle-system execution/rendering for collision/destruction particles;
- collision-spawn requests that are exposed only inside pair damage results but not yet surfaced by the scan aggregate;
- complete reward/score/audio/UI consequence consumption;
- full Player lifecycle/game-over orchestration in the native session;
- original film/InputSprocket action-bit assignments, replay playback, controller/touch, and two-player host behavior;
- persistent entity-owned render-queue record lifetime.

Those are now narrower follow-on tasks. The earlier encounter-overpopulation,
inert-rule, and silent-removal host defects are no longer conflated with them.

## 2026-08-30 playable-host closure note

The host now consumes recovered player-damage/pickup/lifecycle spawn outputs, executes/renders recovered live particle systems, physically prunes finalized member/group history, and validates canonical Plasma Bomb secondary fire. A conservative one-visible-viewport far-offscreen deletion guard is intentionally host-bounded pending exact PPC outer-list-cull caller recovery; it is not claimed as the original threshold. See `docs/LIVE_WORLD_WEAPON_INTEGRATION.md` and `docs/TESTING_VALIDATION.md` for the validated native-playable sequence and stress witness.
