# Player pickup, shield, death, and respawn runtime — Mac 1.0.6

Status: **binary-confirmed clean subset**.

This document covers the player-side gameplay routines reached by the recovered
entity-vs-player collision loop: pickup dispatcher `0x37580`, shield pickup
`0x27490`, money `0x275B0`, extra life `0x26D70`, multiplier `0x29B20`,
player damage `0x27100`, invulnerability getter `0x27DD0`, immediate death
entry `0x27E50`, lifecycle switch `0x2A150`, and respawn initializer `0x29CC0`.
The recovered clean subset now spans pickup through death, life consumption,
respawn entry, entry invulnerability expiry, and game-over disable.

## Compiled Player Definition anchors

The clean `CompiledPlayerRuntimeDefinition` uses source-format names instead of
raw compiled offsets:

| Player Definition key | compiled offset | runtime use |
| --- | ---: | --- |
| `defaultShieldPercentage_INT` | `+0x48` | initial semantic shield percentage |
| `shieldWarningPercentage_INT` | `+0x4C` | one-shot low-shield warning threshold |
| `shieldBaseHitPercentage_INT` | `+0x50` | multiplier applied to incoming entity `damage_FLOAT` |
| `shieldHitDelay_INT` | `+0x54` | player hit-delay ticks |
| hit glow color | `+0x58` | hit feedback color |
| hit glow speed | `+0x5C` | hit feedback speed |
| `life_MaxNum_INT` | `+0x60` | extra-life cap |
| `life_NumInitial_INT` | `+0x64` | initial life count |
| `life_InitialRequiredScore_INT` | `+0x68` | later life/score state machine |
| `life_AdditionalRequiredScore_INT` | `+0x6C` | later life/score state machine |
| `life_Spawn_ID` | `+0x70` | successful extra-life effect |
| `gameOverTime_INT` | `+0x80` | status-1 game-over countdown |
| `dyingTime_INT` | `+0x84` | ordinary status-3 death duration |
| `finalDyingTime_INT` | `+0x88` | final-life status-3 duration |
| `entry_InvulnerabilityTime_INT` | `+0x8C` | status-4 invulnerability duration |
| `entry_soloStartX_INT` | `+0x90` | solo entry X |
| `entry_soloStartY_INT` | `+0x94` | solo entry Y |
| `entry_multiStartX_INT` | `+0x98` | multiplayer entry X |
| `entry_multiStartY_INT` | `+0x9C` | multiplayer entry Y |
| `entry_Spawn_ID` | `+0xA0` | respawn/entry effect |
| `entry_InitialDelay_INT` | `+0xB8` | status-2 waiting duration |
| `death_Spawn_ID` | `+0xBC` | immediate death spawn |
| `active_SpawnOnHit_ID` | `+0xC8` | rate-limited player-hit spawn |
| `active_ShieldWarningObject_ID` | `+0xCC` | low-shield warning spawn |
| `active_DefenceBonusObject_ID` | `+0xD0` | later defence-bonus path |

Canonical Player 1/2 gameplay values are identical for this subset:

```text
default shield       100
warning threshold     15
base hit percentage   15
shield hit delay       1
max lives             10
initial lives          3
life spawn           noel
death spawn          plde
hit spawn            plsh
warning object       nosw
defence bonus        nodb
game-over time         20
dying time             80
final dying time       40
entry initial delay    55
entry invulnerability  60
solo entry           208,330
P1 multi entry       104,330
P2 multi entry       312,330
entry spawn           plen
```

## Fixed Game and Objects table contracts

The executable fetches these by positional index rather than by name. The clean
compiler therefore verifies the labels before accepting the table:

| table | index | canonical label | canonical value |
| --- | ---: | --- | ---: |
| `Game[gafl]` | 161 | `Player_ImpactDamageToEntities` | `100.0` |
| `Game[gafl]` | 162 | `Player_DelayBetweenHitSpawns` | `10.0` |
| `Game[gafl]` | 167 | `Entity_HitDelay` | `1.0` |

The two delay values are converted through the PPC integer-conversion path;
clean code preserves truncation toward zero.

Immediate death `0x27E50` releases held money through fixed `Objects[gaob]`
slots:

| index | label | ID | denomination |
| ---: | --- | --- | ---: |
| 2 | `MoneyUnit_50` | `calg` | 50 |
| 3 | `MoneyUnit_10` | `cals` | 10 |
| 4 | `MoneyUnit_5` | `casg` | 5 |
| 5 | `MoneyUnit_1` | `cass` | 1 |

## Pickup dispatcher `0x37580`

`pickup_Type_ID` is UnitDef `+0x4D4`; `pickup_Value_INT` is `+0x4DC`.
Dispatcher return `1` means the caller consumes/destroys the pickup and skips
ordinary reciprocal collision damage. Return `0` means leave it alive and also
skip ordinary impact damage.

Recovered switch behavior:

- `coin`: if value is nonzero, add it to the player's semantic money counter,
  then invoke pickup feedback. A zero-value coin skips both operations but is
  still accepted/consumed.
- `mult`: apply the exact jump-table ladder `1->2->3->4->5->10`; every other
  current multiplier is a no-op. The pickup remains accepted.
- `exli`: increment lives only when below `life_MaxNum_INT`; a successful
  increment emits `life_Spawn_ID`. At max lives it is still accepted/consumed.
- `shie`: add the integer pickup value to semantic shield and clamp to
  `[0,100]`.
- `air ` / `grnd`: both query live player `+0xCE`; when invulnerable they return
  rejected, otherwise accepted.
- `spec` and the default branch: accepted no-op.

Canonical `Game.pak` contains exactly eight pickup Unit Definitions:

```text
coin  4
mult  1
exli  1
shie  2
air   0
grnd  0
spec  0
```

## Semantic player storage

The original deliberately biases/obfuscates some values in live memory. Clean
code stores semantics directly:

- shield lives at original `+0xA8` behind a fixed floating bias; clean value is
  the actual percentage;
- money lives at original `+0xAC` behind a fixed integer bias; clean value is
  the actual held-money integer;
- lives are likewise recovered through helper routines and are stored directly
  in `PlayerRuntimeSlot`;
- multiplier begins at `1` and follows the jump-table ladder above;
- live `+0xCE` is the invulnerability byte;
- live `+0xD0` is the shield-hit/defence latch;
- live `+0xD1` is the low-shield warning latch;
- live `+0x204/+0x208` are the last shield-hit and last hit-spawn ticks.

These live-player offsets must not be confused with identically numbered Player
Definition offsets.

## Player damage `0x27100`

The recovered order is observable and preserved:

1. Only status `4` enters the damage path.
2. Require `currentTick >= lastShieldHitTick + shieldHitDelay`; an earlier tick
   returns immediately.
3. Store `lastShieldHitTick = currentTick` **before** testing invulnerability.
4. If not invulnerable, calculate
   `scaledDamage = incomingDamage * shieldBaseHitPercentage`, subtract it from
   shield without clamping, and set the live shield-hit latch when the scaled
   amount is positive.
5. Re-read semantic shield. Death occurs only when shield is **strictly below
   zero**. Exactly zero remains alive.
6. If alive, issue the hit-glow call even when invulnerability bypassed shield
   subtraction.
7. For positive original incoming damage, rate-limit `active_SpawnOnHit_ID` by
   `Game[gafl]` 162. Equality passes the gate.
8. If the warning latch is clear and shield is at/below
   `shieldWarningPercentage_INT`, request the warning object and latch the
   warning state. The latch is set even if the resource is `none`.

A consequence of step 3 is that an invulnerable impact still consumes the
player hit-delay window.

## Immediate death entry `0x27E50`

When `0x27100` sees shield `< 0`, the immediate death helper:

- emits `death_Spawn_ID` when present;
- clears hit-spawn/hit-delay/warning bookkeeping;
- decomposes held money in descending `50,10,5,1` order, spawning the fixed
  `calg/cals/casg/cass` resources at the player position;
- clears held money;
- writes player status `3` and the current tick;
- raises invulnerability for an enabled player.

**It does not decrement lives.** Life consumption is performed later by the
status-3 branch of `0x2A150`. Treating `0x27100` as a complete life-loss routine
would be an incorrect modernization of the original control flow.

## Death/respawn lifecycle `0x2A150` / `0x29CC0`

The downstream lifecycle switch is now reconstructed for statuses 1–4:

| status | recovered meaning | timer/action |
| ---: | --- | --- |
| `1` | game-over countdown | after strict `gameOverTime`, clear player enabled `+0xC4` |
| `2` | waiting / entry delay | after strict `entry_InitialDelay`, call respawn initializer |
| `3` | dying | after strict death timer, optionally consume life, then respawn or enter status 1 |
| `4` | active | expire invulnerability after strict `entry_InvulnerabilityTime` when both blocker gates permit |

All lifecycle duration checks use the original signed 32-bit tick arithmetic and
strict `currentTick > statusSince + duration` comparison. Equality still waits.
Status 3 selects `finalDyingTime_INT` only when the semantic life count is exactly
one; every other life count uses `dyingTime_INT`.

The fifth argument to `0x2A150` gates the actual life decrement. Its caller feeds
a global byte that latches to one after Player 1 first reaches active status 4.
The clean API therefore exposes it as `consume_life_on_death`, with the bounded
higher-level meaning that gameplay has started. Before the latch is set, an
expired dying state can transition without charging a life.

When an expired death leaves lives above zero, `0x29CC0` re-enters active status:

- choose `entry_soloStartX/Y` when live `+0xCD == 1`, otherwise the multi pair;
- write x/y directly to the selected entry coordinates;
- set velocity `+0x10/+0x14` from the executable's shared literal `{0.0f,0.0f}`;
- write status 4 and `statusSince = currentTick`;
- emit `entry_Spawn_ID` when present;
- then restore default shield and clear shield-hit/spawn clocks and warning latch.

This is binary evidence that the Player Definition's serialized
`entry_StartVelocity*` values are **not** what `0x29CC0` writes during this
respawn path. The zero vector is a relocated executable constant shared with
other engine routines.

When the post-gate life count reaches zero, the player enters status 1 and stays
enabled through the game-over countdown. Only a later strict expiration of
`gameOverTime_INT` clears enabled `+0xC4`.

Active-status invulnerability has two distinct blockers before it may clear: the
external `0x5CF0` gate and live `+0xCF`. The clean lifecycle function keeps the
former as an explicit `defer_invulnerability_expiry` orchestration input and
models the latter with the existing live latch.

## Clean integration boundary

`player_runtime.cpp` now implements the concrete pickup/stat/shield/death-entry
mutations. `collision_runtime.cpp` intentionally retains narrow callbacks at
its subsystem boundary; callers can bind `apply_legacy_player_pickup()` and
`apply_legacy_player_damage()` while separately consuming their returned spawn,
feedback, warning, and money-drop facts. This keeps world spawning/audio/UI
orchestration explicit rather than hiding it inside collision geometry.

## Validation

Synthetic regression coverage verifies:

- source-to-compiled Player Definition fields;
- fixed Game/Objects positional contracts and label rejection;
- zero/nonzero coin behavior;
- complete multiplier ladder;
- life cap and extra-life spawn;
- shield pickup clamping;
- `air `/`grnd` invulnerability rejection;
- player hit-delay and invulnerability ordering;
- strict negative-shield death versus zero-shield survival;
- hit spawn and warning timing;
- money decomposition/resource IDs;
- no life decrement during immediate death entry;
- status-2 strict entry delay and solo/multiplayer coordinate selection;
- zero-velocity respawn from the executable literal pool;
- ordinary versus final-life dying durations;
- gameplay-start-gated life consumption;
- status-1 game-over delay/disable;
- strict entry-invulnerability expiry and both blocker gates.

The repository suite is **32/32 PASS**. Canonical `Game.pak` remains at 386
constructed groups / 546 live members, construction RNG seed `2249411936`, 544
active after the first player-aware tick, and motion RNG seed `2633739833`.
