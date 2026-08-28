# Unit / weapon / player definition and behavior model

Status: **typed structural reconstruction complete; core state-transition semantics binary-confirmed and implemented.**

## Canonical unit corpus

The 1.0.6 `Game.pak` contains **386** `.unde` unit definitions. Clean parsing
produces:

- 386 unit definitions;
- 1,167 unit states;
- 532 nested spawn sets;
- 5,835 state-rule records;
- 223 distinct observed unit tags.

Every state declares five rule slots, explaining the exact 5,835-rule total.
The parser preserves original field order and explicitly models repeated state,
spawn-set and rule structures.

## Rule conditions: data usage vs executable capability

Canonical 1.0.6 data uses nine non-empty rule-condition strings plus the empty
unused value. The executable itself supports **17** condition strings through
the dispatch routine at code `0x15550`.

Canonical usage:

| Condition | Occurrences |
| --- | ---: |
| empty / unused | 2,861 |
| `Is Tracking Player` | 2,773 |
| `Is Not Active` | 67 |
| `Is Active` | 52 |
| `No Destroyable Ground Entities Are Active` | 41 |
| `Number of This Type of Entity Active` | 17 |
| `No Players Are Active` | 15 |
| `This Entity's Animation Has Stopped` | 6 |
| `No Destroyable Air Entities Are Active` | 2 |
| `Are Fewer of These Entities Active` | 1 |

The seven additional non-empty conditions supported by the executable but not
used by canonical 1.0.6 content are still represented in the clean enum for
cross-version compatibility. See `RULE_CONDITIONS_1_0_6.json` for all 17
strings, handler addresses and predicates.

## Rule ordering

Each state owns five slots. PPC `0x15550` scans them in file order. The first
true predicate calls the action routine and **terminates rule evaluation for
that tick**.

This is true even if the action string is stale/unresolved and therefore does
nothing. The clean `evaluate_first_matching_rule()` reproduces that behavior.

## Action lookup is exact, not case-folded

PPC state-action routine `0x146F0` uses helper `0x57820`; disassembly proves that
helper is ordinary byte-exact `strcmp`.

Actions are therefore:

- no action (`""` / `No State`);
- exact local state-name transition;
- exact `Delete`;
- exact `Destroy`;
- unresolved non-empty label -> **runtime no-op**.

Earlier work temporarily treated state lookup as case-insensitive because the
corpus contains a case-only mismatch. Binary inspection disproved that model and
the clean implementation has been corrected.

With exact lookup, canonical 1.0.6 contains **44 active unresolved/no-op action
occurrences**:

- 15 `Wait for Player Approach` where the local state is actually
  `Wait For Player Approach`;
- 11 `Spawn Screws Mk 3`;
- 9 `Pause Scrolling, Wait, Delete`;
- 5 `Pause, Wait, Spawn Flag, Delete`;
- 2 `Spawn Shield Iris Flag`;
- 1 `RULE - Wait Until Level Cleared`;
- 1 `Track Player, Spawn Flag`.

The 15 case-only mismatches all occur as `Is Active` rule actions. They are not
silently repaired by the remaster fidelity mode.

There are also **30 inert unresolved range-action labels**, all paired with an
exact zero range threshold, so the original range handler cannot fire them.

## Timer, counter and range semantics

These are now recovered from PPC execution paths and implemented in
`state_runtime`:

- timer delay is chosen inclusively from `[stateOnTimerMin, stateOnTimerMax]`;
- the timer fires only when `currentTick == entryTick + delay`;
- every entity has 20 persistent per-state **entry counters**;
- entering a state increments its slot and immediately tests
  `stateOnCounter_INT`;
- a counter is therefore not a frame/tick counter;
- range transition is disabled by exact `0.0f` and otherwise fires on strict
  `measuredDistance < stateOnRange_FLOAT`.

See `ENTITY_STATE_RUNTIME.md` for addresses and detailed ordering.

## Cross-resource integrity

The clean definition database validates proven unit references from unit core
fields, unit state rules/spawns, weapon spawns/power-up targets and player
spawn/object fields. All **785** observed references in these proven namespaces
resolve to one of the 386 known unit IDs, excluding explicit sentinels.

## Weapons and players

The same tagged-text system yields:

- 5 weapon definitions;
- 15 nested weapon-spawn records;
- 2 player definitions.

Their generic fields remain preserved without speculative runtime meaning while
weapon/player execution paths are correlated against the relocated PPC image.

## Binary-confirmed unit invariant

The relocated 1.0.6 `G_UnitDefinitions` loader enforces:

- `numStatesUsed > 0`;
- `numStatesUsed <= 20`.

The clean parser rejects definitions outside **1..20**. Canonical data uses at
most 14 states, while the largest observed `stateOnCounter_INT` threshold is 16.

The definition-loading cluster is anchored around code `0x3CF10`, with the main
unit-resource walk around `0x3D0A0`.

## Next behavior targets

The next runtime work is deliberately narrower now that transition semantics are
stable:

1. spawn-set scheduling/volley semantics and RNG consumption;
2. movement/tracking/rotation state fields;
3. hit/damage/destruction transitions;
4. collision and terrain interaction;
5. replay-driven deterministic validation.
