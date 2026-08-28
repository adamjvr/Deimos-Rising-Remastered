# Unit Spawn Runtime — Deimos Rising 1.0.6

Status: **scheduler, target gate, request geometry, and proven request-option subset binary-confirmed and implemented**.

This document describes the clean-room reconstruction of the per-state spawn system used by the Mac 1.0.6 PowerPC executable.

## Primary PPC routines

- `0x15B40` — per-entity spawn scheduler, target gate, geometry, and spawn-request construction.
- `0x17CB0` — initializes every spawn set when a state is entered.
- `0x17150` — rotation-update gate; includes pause-while-spawning logic.
- `0x16BD0` — parent-point visible-rectangle predicate.
- `0x161C0` — current entity heading helper.
- `0x33220` — consumes the completed 44-byte request and enters entity/world construction.
- `0x46580` — inclusive integer RNG mapper.
- `0x553E0` — original 15-bit LCG RNG.
- `0x42920` — startup construction of the 360-entry sine/cosine tables.
- `0x42EE0` / `0x42F00` — cosine/sine table lookups.

## Spawn-set serialized/in-memory layout

The parsed `UnitSpawnSet` fields line up with the structure consumed by `0x15B40`:

| Offset | Serialized field |
|---:|---|
| `+0x20` | `stateSpawnSetSpawn_ID` |
| `+0x24` | `stateSpawnSetXOffset_INT` |
| `+0x28` | `stateSpawnSetYOffset_INT` |
| `+0x2C` | `stateSpawnSetRateMin_INT` |
| `+0x30` | `stateSpawnSetRateMax_INT` |
| `+0x34` | `stateSpawnSetNumInVolleyMin_INT` |
| `+0x38` | `stateSpawnSetNumInVolleyMax_INT` |
| `+0x3C` | `stateSpawnSetDelayBetweenEntitiesMin_INT` |
| `+0x40` | `stateSpawnSetDelayBetweenEntitiesMax_INT` |
| `+0x44` | `stateSpawnSetAdjustOffsetForUnitRotation_BOOL` |
| `+0x45` | `stateSpawnSet_AbsoluteCoordinates_BOOL` |
| `+0x46` | `stateSpawnSetRepeatSpawns_BOOL` |
| `+0x47` | `stateSpawnSetDon'tSpawnOffscreen_BOOL` |
| `+0x48` | `stateSpawnSetPauseAnyRotationWhileSpawning_BOOL` |
| `+0x4C` | `stateSpawnSetTimeToPauseRotationAfterSpawning_INT` |
| `+0x50` | `stateSpawnSetSpawnIfFleeing_BOOL` |
| `+0x51` | `stateSpawnSetSetHeading_BOOL` |
| `+0x54` | `stateSpawnSetHeadingDegrees_INT` |
| `+0x58` | `stateSpawnSet_StationaryOption_BOOL` |
| `+0x59` | `stateSpawnSet_TerrainEffectsOption_BOOL` |

## 24-byte runtime record

The original entity owns a parallel runtime record for every spawn set in the current state:

| Offset | Clean name | Meaning |
|---:|---|---|
| `+0x00` | `rate_delay` | Selected rate before a repeating volley may be armed. |
| `+0x04` | `rate_anchor_tick` | State-entry tick initially; replaced when a repeat re-arms. |
| `+0x08` | `remaining_in_volley` | Members still pending. |
| `+0x0C` | `initial_volley_size` | Selected size of the current volley. |
| `+0x10` | `inter_entity_delay` | Countdown before the next member. |
| `+0x14` | `active` | Scheduler-enabled byte. |

The allocation stride is exactly 24 bytes.

## State-entry initialization (`0x17CB0`)

For every non-`none` set, RNG is consumed in this exact order:

1. choose `rate_min..rate_max`;
2. choose `num_in_volley_min..max`;
3. copy that value to `remaining_in_volley`;
4. choose `delay_between_entities_min..max`.

`rate_anchor_tick = current_tick` and `active = (rate_delay >= 0 && volley_size > 0)`.

A `none` spawn consumes no RNG. State entry also writes `time_to_pause_rotation_after_spawning` into the parent's rotation-pause slot. Canonical 1.0.6 has no non-zero values for that field.

## Per-update scheduler (`0x15B40`)

The set is skipped without changing counters if it is `none`, inactive, has a negative selected rate, or the parent is fleeing while `spawn_if_fleeing` is false.

### Offscreen cancellation

`dont_spawn_offscreen` is consulted only for a full/unstarted volley:

```text
remaining > 0 && remaining >= initial_volley_size
```

If the parent is offscreen, `remaining_in_volley` becomes zero. A partially emitted volley continues even if the parent subsequently leaves the visible rectangle.

### Pending volley member

When `remaining_in_volley > 0`:

1. decrement `inter_entity_delay` if positive;
2. return if it remains positive;
3. emit one due event;
4. decrement `remaining_in_volley`;
5. choose a fresh inter-entity delay.

Step 5 occurs after the final member too and therefore can consume RNG after a volley has ended.

### Completion and repeat re-arm

A one-shot set deactivates on the update after the last member has reduced `remaining_in_volley` to zero.

A repeating set waits until signed `current_tick >= rate_anchor_tick + rate_delay`. Re-arming uses a different RNG order from state entry:

1. choose new inter-entity delay;
2. choose new volley size and copy it to remaining;
3. choose the *next* rate delay.

The first member is not emitted during the same invocation that re-arms the volley.

## Reversed RNG endpoints are original behavior

`0x46580` does not normalize endpoints. It performs signed PPC division/remainder using:

```text
width = max - min + 1
q = RNG15 / width
remainder = RNG15 - q*width
result = min + remainder
```

Canonical `Level 8 - Mid 2[08m2]`, spawn set `Screws Mk 2 - Top`, contains `rate 110..20`. Its signed width is `-89`; the original consequently produces 110..198. The clean runtime preserves this malformed-data behavior.

## Target Unit Definition gate (`0x15D8C..0x15DAC`)

This gate is now tied directly to the original Unit Definition parser rather than inferred from names.

`0x3FDA0` maps:

- `#adjustInitialLocForOwnerScale_BOOL` -> compiled UnitDef byte `+0x12E`;
- `#terrainEffect_BOOL` -> compiled UnitDef byte `+0x132`;
- `#canBeSpawnedOnlyWhenPlayersActive_BOOL` -> compiled UnitDef byte `+0x12A`.

The spawn routine reads **`+0x132`**, therefore this gate is definitively the terrain-effect flag, not the player-active flag.

Behavior:

```text
if !target.terrainEffect:
    eligible
else if parent.stationary:
    reject
else if !parent.terrainEffectsEnabled:
    reject
else:
    eligible
```

The parent bytes are entity `+0x13C/+0x13D`. The entity-construction path copies the inherited request option bytes into those locations, linking them to stationary and terrain-effects behavior.

This explains why `Tank - Tracks[tatr]` can be the sole canonical `terrainEffect=TRUE` unit even though the *track spawn sets themselves* use `TerrainEffectsOption=FALSE`: eligibility depends on the parent tank entity's inherited option, not the child spawn set's option.

## Spawn position and heading (`0x15E18..0x16158`)

### Unrotated offsets

When `adjust_offset_for_unit_rotation == false`:

- relative coordinates begin from the parent x/y;
- absolute coordinates begin from zero;
- x/y integer offsets are added directly;
- if parent scale differs from exactly `1.0f` and target `adjustInitialLocForOwnerScale` is true, offsets are multiplied by parent scale first.

Canonical 1.0.6 has zero Unit Definitions enabling `adjustInitialLocForOwnerScale`, but the executable path is implemented for compatibility.

### Rotation-adjusted offsets

When rotation adjustment is enabled, the parent position is always the base. Canonical data never combines rotation-adjustment and absolute-coordinate flags, but executable branch order is preserved.

The angle starts with current parent heading. When `SetHeading` is enabled, `heading_degrees` is added and values greater than 359 have 360 subtracted **once**. That resulting heading is both the child heading and the angle used to rotate offsets.

The original operation sequence is:

```text
ySin = float(yOffset * scale) * sin(angle)
yCos = float(yOffset * scale) * cos(angle)
rotX = fmsubs(float(xOffset * scale), cos(angle), ySin)
rotY = fmadds(float(xOffset * scale), sin(angle), yCos)
integerX = fctiwz(rotX)
integerY = fctiwz(rotY)
spawn = parent + integer offset
```

The clean implementation preserves single-precision rounding points, fused operation ordering, and truncation toward zero.

## Legacy trig table (`0x42920`)

At startup the executable builds 360-element float cosine and sine tables. For degree `d`:

```text
angle = float(d) * bit_cast<float>(0x3C8EFA35)
cos[d] = frsp(MathLib::cos(angle))
sin[d] = frsp(MathLib::sin(angle))
```

`0x3C8EFA35` is `0.01745329238474369f`. Lookup helpers special-case heading exactly 360 to entry 0.

The clean core preserves the exact embedded input constant, call/table order, and float rounding boundaries. It currently uses the host C++ math library for `sin/cos`; a future replay/emulation comparison can determine whether classic Mac MathLib differs by any final-table ULPs. If necessary, the canonical table can be captured/baked without changing the surrounding API.

## Portable spawn request seed

The clean core now turns a scheduler `spawn_due` event into a portable proven request subset after resolving the target Unit Definition:

- target FourCC;
- x/y;
- heading-set byte and heading;
- child stationary option;
- child terrain-effects option.

The original request also contains parent pointer/owner identity and other fields that belong to the upcoming world/entity-constructor reconstruction. Those are intentionally not guessed here.

## Canonical corpus statistics

Across 532 canonical spawn sets:

- repeating: 218;
- absolute coordinates: 144;
- rotation-adjusted: 73;
- offscreen guarded: 52;
- spawn while fleeing: 2;
- explicit heading: 78;
- pause rotation while spawning: 0;
- child terrain-effects option: 16;
- child stationary option: 0;
- reversed numeric ranges: 1.
