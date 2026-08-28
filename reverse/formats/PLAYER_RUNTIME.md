# Player runtime reverse-engineering notes — Mac 1.0.6

Status: **binary-confirmed subset implemented in the clean core**.

Primary PPC anchors:

| address | recovered role |
| ---: | --- |
| `0x26D70` | add extra life up to PlayerDef max; optional life spawn |
| `0x27100` | player shield/damage path |
| `0x27490` | add/clamp shield pickup |
| `0x27540/0x27560` | semantic shield get/set around biased live storage |
| `0x275B0` | add semantic money around biased live storage |
| `0x27DD0` | read player invulnerability byte `+0xCE` |
| `0x27E50` | immediate death-entry effects/state |
| `0x29B20` | multiplier jump-table update |
| `0x29CC0` | player respawn/entry initializer |
| `0x2A150` | status 1–4 lifecycle switch |
| `0x37580` | pickup dispatcher |

## Player Definition layout subset

| source key / semantic | offset |
| --- | ---: |
| `defaultShieldPercentage_INT` | `+0x48` |
| `shieldWarningPercentage_INT` | `+0x4C` |
| `shieldBaseHitPercentage_INT` | `+0x50` |
| `shieldHitDelay_INT` | `+0x54` |
| hit glow color | `+0x58` |
| hit glow speed | `+0x5C` |
| `life_MaxNum_INT` | `+0x60` |
| `life_NumInitial_INT` | `+0x64` |
| `life_InitialRequiredScore_INT` | `+0x68` |
| `life_AdditionalRequiredScore_INT` | `+0x6C` |
| `life_Spawn_ID` | `+0x70` |
| `gameOverTime_INT` | `+0x80` |
| `dyingTime_INT` | `+0x84` |
| `finalDyingTime_INT` | `+0x88` |
| `entry_InvulnerabilityTime_INT` | `+0x8C` |
| `entry_soloStartX_INT` | `+0x90` |
| `entry_soloStartY_INT` | `+0x94` |
| `entry_multiStartX_INT` | `+0x98` |
| `entry_multiStartY_INT` | `+0x9C` |
| `entry_Spawn_ID` | `+0xA0` |
| `entry_InitialDelay_INT` | `+0xB8` |
| `death_Spawn_ID` | `+0xBC` |
| `active_SpawnOnHit_ID` | `+0xC8` |
| `active_ShieldWarningObject_ID` | `+0xCC` |
| `active_DefenceBonusObject_ID` | `+0xD0` |

## Unit pickup layout

| Unit Definition key | offset |
| --- | ---: |
| `pickup_Type_ID` | `+0x4D4` |
| `pickup_Value_INT` | `+0x4DC` |

`0x37580` returns accepted by default. `air `/`grnd` are the only recovered
rejection branches and return zero while live player `+0xCE` is set. `coin`,
`mult`, `exli`, `shie`, `spec`, and unknown/default values return accepted.

Canonical corpus: 4 `coin`, 1 `mult`, 1 `exli`, 2 `shie`, no `air `/`grnd`/`spec`.

## Player damage ordering

`0x27100` performs:

```text
if status != 4: return
if currentTick < lastHitTick + PlayerDef.shieldHitDelay: return
lastHitTick = currentTick
if !invulnerable:
    scaled = incomingDamage * PlayerDef.shieldBaseHitPercentage
    shield -= scaled
    if scaled > 0: live +0xD0 = 1
if shield < 0:
    0x27E50(player,currentTick)
    return
hitGlow(PlayerDef +0x58/+0x5C)
if incomingDamage <= 0: return
rate-limit PlayerDef.active_SpawnOnHit with Game[gafl] 162
if !warningLatch && shield <= PlayerDef.shieldWarningPercentage:
    optionally spawn PlayerDef.active_ShieldWarningObject
    warningLatch = 1
```

The hit tick is therefore committed before invulnerability. Exactly zero shield
does not die.

## Fixed table positions

`Game[gafl]`:

- 161 `Player_ImpactDamageToEntities` = canonical `100.0`;
- 162 `Player_DelayBetweenHitSpawns` = canonical `10.0`, `fctiwz` before tick math;
- 167 `Entity_HitDelay` = canonical `1.0`, `fctiwz` for entity collision damage.

`Objects[gaob]` money drops used by `0x27E50`:

- 2 `MoneyUnit_50` -> `calg`;
- 3 `MoneyUnit_10` -> `cals`;
- 4 `MoneyUnit_5` -> `casg`;
- 5 `MoneyUnit_1` -> `cass`.

The death helper repeatedly subtracts each denomination and spawns its fixed
resource, then resets the encoded money field, sets status `3`, stores the tick,
and raises invulnerability for an enabled player.

## Player lifecycle `0x2A150`

The lifecycle switch is now recovered as:

```text
status 1: if tick > statusSince + gameOverTime, enabled = 0
status 2: if tick > statusSince + entryInitialDelay, call 0x29CC0
status 3: wait dyingTime, or finalDyingTime when lives == 1;
          optionally decrement life; respawn if lives > 0, else status = 1
status 4: if invulnerable and both blocker gates permit,
          clear invulnerability when tick > statusSince + entryInvulnerabilityTime
```

Every duration comparison is strict; equality waits. The life-decrement gate is
fed by a global byte that becomes latched after Player 1 first reaches status 4,
which bounds its meaning as a gameplay-start gate.

`0x29CC0` chooses solo coordinates from PlayerDef `+0x90/+0x94` when live
`+0xCD == 1`, otherwise multi coordinates `+0x98/+0x9C`. It writes position,
loads velocity from a relocated executable `{0.0f,0.0f}` literal, writes status
4/current tick, and emits `entry_Spawn_ID` from `+0xA0`. The serialized
`entry_StartVelocity*` values are therefore not the values written by this
respawn path. After a death respawn, `0x2A150` restores default shield and clears
the hit clocks and warning latch.

`0x27E50` remains correctly separated from this logic: it enters status 3 but
does **not** decrement lives itself.
