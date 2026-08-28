# Player Target / Motion Runtime — Deimos Rising 1.0.6

This document records the currently recovered player-target and live-member motion behavior from the 1.0.6 PowerPC executable and canonical Unit Definition corpus.

## Player slots and closest-target query

The original player subsystem exposes exactly two slots. A player participates in closest-target queries only when its status byte is `4`. The query returns the player's own signed player-index byte, not an inferred slot number.

Distance uses the legacy integer-gated square-root path: squared single-precision distance is truncated to an integer before square root. Replacement uses strict `<`, so an exact tie remains with the first active slot.

The clean runtime represents this contract as `PlayerWorld` / `ClosestPlayerResult`.

## Recovered live-member motion block

Direct PPC correspondence currently maps:

- live `+0x108/+0x10C` — target velocity X/Y;
- live `+0x110/+0x114` — per-axis velocity delta;
- live `+0x118` — target player index;
- live `+0x11C/+0x120` — target player X/Y;
- live `+0xCC` — runtime fleeing flag selecting PPC `0x16CC0`.

The clean runtime also keeps the most recently measured target distance because the range-transition layer consumes that same query result.

## Dispatcher ordering

Recovered `0x15280` ordering:

1. if already fleeing, execute Flee update and leave the dispatcher early;
2. otherwise refresh closest active player;
3. apply no-player lifecycle behavior;
4. execute Hunt;
5. evaluate range transition;
6. reload current state after any range transition;
7. execute Hold Position / Cyclic Motion / Flee as applicable;
8. perform ordinary velocity convergence.

The caller then performs Lock / Link / Orbit owner-location behavior and the spawn scheduler.

## Hunt (`0x16FE0`)

Hunt does not point directly at the stored player position. It constructs a random velocity envelope using the original LCG in this order:

1. `coarse = randomInclusive(trunc(stateHoldMaxSpeed)/2, trunc(stateHoldMaxSpeed))`;
2. `fine = randomInclusive(1, 100) / 100.0f`;
3. `envelope = coarse + fine`.

Each axis is clamped against `+/-envelope`; hitting a bound reverses that axis's velocity delta; the delta is then applied. The resulting current velocity is copied to the target-velocity fields.

Hunt still consumes these RNG draws when no player is active unless a Delete/Destruct-on-no-player action has already removed the member.

## Hold Position (`0x17C40`)

Hold uses the stored closest-player position/distance and deliberately negates the normalized target displacement:

`targetVelocity = -(target - entity) / distance * stateHoldMaxSpeed`

This sign is preserved even though the serialized state name can suggest a more conventional 'hold near target' interpretation.

## Cyclic Motion (`0x17B70`)

Cyclic Motion uses `stateMaxSpeed_FLOAT` and `stateDelta_FLOAT`. Per-axis acceleration is chosen from which side of the stored target the member occupies; current velocity is advanced immediately and clamped to the configured maximum.

## Flee (`0x16CC0`)

The live fleeing flag selects the Flee path. Flee uses `stateFleeSpeed_FLOAT` / `stateFleeDelta_FLOAT` and accelerates each axis away from the stored target while clamping to the configured maximum.

The transition/caller that raises the runtime fleeing flag remains a separate compatibility-reconstruction target; the proven Flee motion itself is implemented.

## No-active-player lifecycle

`stateDeleteOnNoActivePlayers_BOOL` marks the live member deleted.

`stateDestructOnNoActivePlayers_BOOL` enters the destruction lifecycle path. The clean runtime records `destroyed`; downstream audio/particle/score effects of the larger original destruction routine remain separate reconstruction work.

Canonical 1.0.6 contains 9 Delete-on-no-player states and 9 Destruct-on-no-player states.

## Canonical corpus coverage

Canonical 1.0.6 contains:

- 30 Hunt states;
- 7 Hold-to-target states;
- 40 Cyclic-motion states;
- 9 Delete-on-no-player states;
- 9 Destruct-on-no-player states.

A shared-RNG construction pass creates 546 live members from 386 Unit Definition requests. With two deterministic active player slots, a first player-aware tick at tick 0 removes exactly two members, both through zero-delay timer actions:

- `Ground Obstacle[grob]` — Destroy;
- `Tank - Pulse Track Flag[tptf]` — Delete.

No first-tick removal in that regression comes from player targeting, Hunt/Hold/Cyclic/Flee, rules, or range.

## Remaining boundary

Collision candidate scanning at `0x36CF0`, hit/damage/destruction consequences, the trigger that raises the fleeing runtime flag, and full integration with replay-driven world position updates remain the next behavior-reconstruction boundary.
