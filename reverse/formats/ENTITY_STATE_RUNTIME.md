# Entity State Runtime — 1.0.6 PPC-confirmed semantics

This document records behavior recovered from the relocated PowerPC executable,
not merely inferred from the tagged-text field names.  Code offsets refer to the
1.0.6 PEF code section.

## Core routines

| Routine | Code offset | Recovered role |
| --- | ---: | --- |
| state transition/action | `0x146F0` | Resolve `Delete`, `Destroy`, or exact local state name; initialize entered state |
| range transition | `0x15280` | Player-distance/range handling and `stateOnRange*` transition |
| rule evaluator | `0x15550` | Evaluate five rule slots in order via 17-condition dispatch |
| animation update | `0x15930` | Advances animation and maintains animation-stopped runtime flag |
| destroy entity | `0x16300` | Destruction path used after transition result |
| inclusive integer RNG | `0x46580` | `min + rng % (max-min+1)` |
| base RNG | `0x553E0` | 15-bit LCG result |
| string compare | `0x57820` | byte-exact `strcmp` |

## State-name/action lookup is byte-exact

The transition routine compares strings with `0x57820`.  That helper is a
straight byte-for-byte, case-sensitive C string comparison.

Action handling is:

1. exact `Delete` -> set delete result and return;
2. exact `Destroy` -> set destroy result and return;
3. scan local state names with exact comparison;
4. if a state matches, enter it;
5. if no state matches, return without changing state.

Therefore an unknown/non-matching action label is a **runtime no-op**, not a
hidden command.  This also means canonical case-only mismatches are genuine
no-ops.  The 1.0.6 corpus contains 15 occurrences of:

`Wait for Player Approach`

where the local state is:

`Wait For Player Approach`

The clean implementation must preserve the mismatch rather than correcting it.

## Rule evaluation

Each state contains five fixed-size rule slots.  The runtime scans them in file
order.

For each active slot:

1. validate/resolve its Unit Definition ID;
2. identify its condition by exact comparison against the 17-entry condition table;
3. evaluate the corresponding predicate;
4. when a predicate is true, call the state-action routine;
5. **exit the rule loop immediately**.

The loop exits after the first true condition even if its action is an
unresolved/no-op string.  A later rule does not get a chance to fire that tick.

The complete 17-condition dispatch, handler addresses and canonical occurrence
counts are recorded in `reverse/inventories/RULE_CONDITIONS_1_0_6.json`.

### Range-rule zero quirk

For both:

- `This Entity is Within Range of a Player`
- `This Entity is Not Within Range of a Player`

`rule.range == 0` bypasses the world query and leaves the predicate false.
Consequently the second form is **not** simply logical negation when the range
is zero: both predicates are false.

### Required-level comparisons

Visibility, tint and scale conditions compare their current and required float
values with PPC floating equality.  The clean core intentionally uses exact
float equality; it does not add an epsilon.

## Timer transition

When a state is entered, `0x146F0` reads the two timer bounds and calls
`0x46580`.  That helper is exactly:

```text
if min == max:
    delay = min
else:
    delay = min + RNG() % (max - min + 1)
```

The base RNG at `0x553E0` is the familiar 32-bit LCG update
`seed = seed * 1103515245 + 12345`, returning bits 16..30 as a 15-bit value.

The chosen delay is stored on the entity together with the state-entry tick.
The main entity update later performs:

```text
currentTick == stateEntryTick + chosenDelay
```

This is exact equality, **not `>=`**.  Reproducing deterministic films therefore
requires preserving tick cadence and RNG consumption order.

All 1,167 canonical states have timer bounds with `min <= max`.

## State-entry counter transition

An entity owns a fixed array of **20 state-entry counters**, one per possible
unit state.  The 20-slot size agrees with the independently recovered
`G_UnitDefinitions` invariant that a unit has 1..20 states.

On state entry:

1. set the current state index;
2. increment that state's persistent entry-count slot;
3. if `stateOnCounter_INT > 0` and the new count equals it, process
   `stateOnCounterChangeTo_STR` immediately.

It is therefore **not a frame/tick counter**.  It counts how many times that
entity has entered a particular state.

For a non-empty attempted state transition, the original zeros the current
state's entry-count slot before recursively entering the target.  This includes
a non-empty stale/unresolved label.  `Delete`/`Destroy` return through their
entity-lifetime paths instead.

The largest counter threshold in canonical 1.0.6 data is 16.

## Range transition

`0x15280` computes player/distance information, then checks the current state's
range field.  The transition is enabled when the configured float is not
exactly `0.0` and triggers when:

```text
measuredDistance < stateOnRange_FLOAT
```

The comparison is strict `<`, not `<=`.

## Per-tick ordering established so far

Within the main entity update around `0x33C58` the relevant sequence is:

1. timer transition check/action;
2. refresh current-state pointer if a timer action changed state;
3. animation update (`0x15930`);
4. five-slot rule evaluator (`0x15550`) when the state enables rules;
5. refresh current-state pointer if a rule changed state;
6. apply/update additional state-driven visual/motion properties;
7. other entity/world handling;
8. range transition handler (`0x15280`);
9. continue remaining movement/collision/spawn work unless delete/destroy exits.

This ordering matters because animation can set the `Animation Has Stopped`
flag immediately before rules inspect it, while range transitions occur later
than rule evaluation in the same entity tick.

## Clean-core implementation

`include/deimos/state_runtime.hpp` and `src/core/state_runtime.cpp` currently
encode only behavior proven above:

- exact inclusive integer mapping;
- state-entry tick and timer delay;
- persistent 20-slot entry counters;
- immediate counter-threshold detection;
- exact-equality timer due test;
- strict range transition predicate;
- counter reset behavior for attempted state transitions.

World-dependent rule queries remain fact/provider inputs until the entity world
model is reconstructed.  This prevents physics/AI guesses from contaminating
the now-proven transition kernel.
