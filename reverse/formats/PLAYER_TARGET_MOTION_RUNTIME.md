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
3. apply no-player Delete/Destruct behavior;
4. if no player is active, apply UnitDef north/south flee flags and leave the dispatcher immediately;
5. execute Hunt;
6. evaluate range transition;
7. reload current state after any range transition;
8. execute Hold Position / Cyclic Motion;
9. perform ordinary velocity convergence.

A range transition may enter an explicit flee state in step 6. State entry raises the
live flee latch immediately, but PPC `0x1550C` still performs ordinary convergence
for the remainder of that dispatch. The next tick begins at step 1 and executes the
flee accelerator. The caller then performs Lock / Link / Orbit owner-location behavior
and the spawn scheduler.

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

The live fleeing flag selects the Flee path. Flee uses `stateFleeSpeed_FLOAT` /
`stateFleeDelta_FLOAT` and accelerates each axis **toward** the authored destination
stored at live `+0x11C/+0x120`, clamping each velocity component to +/- flee speed.
An active player target is not required while the latch is set. Equality takes the
negative-delta branch, matching the PPC compare/branch sequence.

### Flee target initializer (`0x17510`)

`stateFlee_ID` is executable behavior. The initializer raises live `+0xCC` first, then
dispatches the FourCC and writes a destination using `Game[gafl]` world/viewport values:

| FourCC | Destination |
|---|---|
| `nora` | random X in 0..visible width, north boundary |
| `sora` | random X, south boundary |
| `wera` | west boundary, random Y in 0..visible height |
| `eara` | east boundary, random Y |
| `noce` / `soce` | center X, north / south boundary |
| `wece` / `eace` | west / east boundary, center Y |
| `opve` | random X, opposite vertical edge from current half |
| `opho` | opposite horizontal edge from current half, random Y |
| `rave` | random north/south choice, then random X |
| `raho` | random east/west choice, then random Y |
| `cega` | center of the visible game area |

Canonical 1.0.6 values are north=-1000, south=2000, west=-1000, east=2000,
visible width=416 and visible height=480. The order of random draws is part of the
shared replay RNG contract. Unknown FourCCs leave the flee latch set and preserve the
previous destination, because the original raises the latch before dispatch.

State entry at `0x146F0` invokes `0x17510` for non-`none` `stateFlee_ID` before
spawn-runtime initialization. Entering a non-flee state clears the latch only if the
state being left itself had an explicit flee ID; this preserves the independent
UnitDef no-active-player flee latch. UnitDef `fleesNorthOnNoActivePlayers_BOOL` and
`fleesSouthOnNoActivePlayers_BOOL` invoke `nora`/`sora` respectively, with north
precedence if malformed content enables both.

## No-active-player lifecycle

`stateDeleteOnNoActivePlayers_BOOL` marks the live member deleted.

`stateDestructOnNoActivePlayers_BOOL` enters the destruction lifecycle path. The clean runtime records `destroyed`; downstream audio/particle/score effects of the larger original destruction routine remain separate reconstruction work.

Canonical 1.0.6 contains 9 Delete-on-no-player states and 9 Destruct-on-no-player states.

## Canonical corpus coverage

Canonical 1.0.6 contains:

- 30 Hunt states;
- 7 Hold-to-target states;
- 40 Cyclic-motion states;
- 17 states with explicit non-`none` flee modes;
- 8 Unit Definitions that flee north when no player is active;
- 1 Unit Definition that flees south when no player is active;
- 9 Delete-on-no-player states;
- 9 Destruct-on-no-player states.

A shared-RNG construction pass creates 546 live members from 386 Unit Definition requests. With two deterministic active player slots, a first player-aware tick at tick 0 removes exactly two members, both through zero-delay timer actions:

- `Ground Obstacle[grob]` — Destroy;
- `Tank - Pulse Track Flag[tptf]` — Delete.

No first-tick removal in that regression comes from player targeting, Hunt/Hold/Cyclic/Flee, rules, or range.

## Remaining boundary

Canonical target/Hunt/Hold/Cyclic/flee/convergence behavior is now closed for shipped
1.0.6 data. One state byte consulted by the non-flee branch of `0x16CC0` at compiled
state `+0x349` remains intentionally unnamed; no canonical state enables it, so it has
no shipped-game effect and is retained as a compatibility-research item rather than
assigned a guessed semantic. Replay/InputSprocket-driven target behavior remains a
separate host/input reconstruction boundary.
