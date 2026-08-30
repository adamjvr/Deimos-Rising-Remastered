# Live level activation, entity-rule AI, and player lifecycle

Status: **integrated for the external-original-data live Level-1 session.**

This milestone corrects the first live-world integration, which had mistakenly
constructed every serialized Level object at startup. The original executable
already exposed the correct scheduling boundary through the recovered terrain
runtime: PPC `0xFA10` primes world routine `0x33090(world_y)` from the initial
source bottom through `sourceTop-64`, and PPC `0x10000` invokes the same world
routine for exactly one newly exposed row after each vertical scroll step.

## Exact row activation

`LevelObjectActivationRuntime` preserves the serialized Level object list and
activates an object once, in file order, only when its exact `object.y` row is
visited by the terrain callback.

For canonical Level 1 (`le01`, Kepler Massif):

- background height: 3600;
- initial source view: Y=3120..3600;
- recovered activation margin: 64 pixels;
- initial activation band: Y=3056..3600 inclusive;
- serialized placed objects: 46;
- objects in the initial activation band: exactly 2;
- both initial objects are `bsde` Bonus Station - Desert at Y=3309 and Y=3129;
- the next placed object is `fl02` at Y=3020 and activates only when scrolling
  reaches its row (tick 36 at the canonical one-pixel/tick terrain rate).

The terrain callback runs after `source_view` moves. `tick_live()` therefore
records the member count before scrolling and shifts only those pre-existing
members by the terrain delta. Entities constructed by the newly exposed row
already used the new world-Y origin and are not shifted a second time.

## World rule facts / AI

The live entity tick now supplies the world facts consumed by the already
recovered PPC `0x15550` five-slot rule interpreter:

- matching Unit-ID active query, global for range 0 or strict `< range` when a
  spatial range is present;
- matching Unit-ID tracking-player query;
- global active count by Unit ID;
- destroyable air/ground presence;
- active-player presence;
- strict player-range fact;
- current sprite visibility/tint/scale required-value facts.

Serialized `none`, `NULL`, and empty Unit IDs match no world member. This is
important for the canonical corpus: all 2,773 `Is Tracking Player` filler rules
use Unit ID `none` and action `Delete`; treating that sentinel as an arbitrary
or current entity would catastrophically delete normal actors.

The six canonical `Animation Has Stopped` rules remain explicitly false in this
live bridge until sprite-frame animation stop semantics are independently
reconstructed. No invented animation completion rule is introduced.

Real Level-1 validation now observes the first `fl02` group enter from the
64-pixel pre-roll and progress from its initial southward state into Hunt Player
and later Hunt Player Faster states while refreshing Player-1 targets.

## Authoritative player lifecycle

The live tick now runs the recovered player lifecycle before host movement.
Dying status therefore expires through the original dying/final-dying timers,
consumes lives through the recovered gate, restores shield/position on respawn,
and eventually reaches game-over/disable instead of leaving Player 1
permanently stuck in dying status.

Host-bridged weapons are accepted only while the authoritative player is
enabled and status 4 (active). This closes the observed WIP failure where a
collision could stop movement while firing continued indefinitely.

## Regression gates

Synthetic suite: **53/53 PASS**.

External-data `deimos_original_frame_probe` additionally asserts:

- live-world initial placed objects: 2;
- live-world initial members/active members: 2 / 2;
- live-world right-input tick: Player `(209.6,330)`, velocity `(1.6,0)`, status 4;
- placed objects activated by tick 120: 3;
- first Flipper encounter has entered a non-initial state with an active player target;
- Player 1 remains active at tick 120;
- preserved canonical baseline frame hashes;
- corrected live-world integration hashes.

Corrected live-world witnesses:

- initial: `0x864f27d9c3820d7f`;
- air-fire tick 1: `0xe94cfb91faa42e72`;
- tick 120: `0x8a8f770b4de4d2cb`.

These are clean-integration regression witnesses, not original executable
screenshot captures.

## Remaining boundaries

- sprite animation-stop semantics for the six rules that consume it;
- complete offscreen member cleanup (long projectile soaks still retain
  projectiles after they leave the visible region);
- destruction reward/effect/audio/UI orchestration;
- persistent entity-owned render queue records;
- exact InputSprocket / film-bit assignments.
