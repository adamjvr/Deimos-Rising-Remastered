# Unit Definition compiled-memory anchors — Mac 1.0.6

Status: **binary-confirmed anchors used by runtime reconstruction**.

The canonical `.unde` text is parsed by the PPC routine beginning at `0x3FDA0`. The loader allocates `0x7A60` bytes for the complete compiled definition object and uses a base at object `+0x18` for much of the scalar field table. Because the parser call sites expose both the literal serialized key string and the destination offset, individual runtime bytes can be tied to exact source tags without semantic guessing.

## Spawn-relevant booleans

| Serialized key | Parser call | Destination from `object+0x18` | Compiled offset | Runtime evidence |
|---|---:|---:|---:|---|
| `#canBeSpawnedOnlyWhenPlayersActive_BOOL` | `0x40064` | `+0x112` | `+0x12A` | Distinct from the pre-spawn terrain gate. |
| `#adjustInitialLocForOwnerScale_BOOL` | `0x3FFBC` | `+0x116` | `+0x12E` | Read at `0x15E5C` / `0x15FEC` before scaling spawn offsets. |
| `#terrainEffect_BOOL` | `0x3FF44` | `+0x11A` | `+0x132` | Read at `0x15D8C` by the terrain-effect parent-option gate. |

The key strings themselves are recovered from relocated initialized data reached through the executable's TOC. These offsets eliminate an earlier tentative misidentification of `+0x132` as `canBeSpawnedOnlyWhenPlayersActive`.

## Confidence rule

Only offsets with a direct parser-key-to-destination call site are named here. Nearby bytes remain opaque until equivalent evidence is recovered.
## Constructor-facing anchors

The same parser/disassembly correlation now establishes the fields used by the normal entity constructor and initial-motion routines:

| Serialized key | Compiled UnitDef offset | Runtime consumer |
| --- | ---: | --- |
| `#doNotSpawnIfTypeAlreadyExists_BOOL` | `+0x118` | top constructor `0x33304` |
| `#deleteExistingEntitiesOfThisTypeOwnedByPlayer_BOOL` | `+0x119` | top constructor `0x33364` |
| `#initiallyHuntsClosestPlayer_BOOL` | `+0x11F` | initial motion `0x37BE4` |
| `#doBurst_BOOL` | `+0x122` | initial motion `0x37C88` |
| `#doImplode_BOOL` | `+0x123` | initial motion `0x37C94` |
| `#initialHeadingSetInEditor_BOOL` | `+0x124` | top/member/initial-motion heading route |
| `#useOwnerHeading_BOOL` | `+0x129` | initial motion `0x37D4C` |
| `#randomiseInitialLoc_BOOL` | `+0x12D` | initial position `0x3799C` |
| `#numInGroupMin_INT` | `+0x194` | group selection `0x369F0` |
| `#numInGroupMax_INT` | `+0x198` | group selection `0x369F0` |
| `#groupDelayMin_INT` | `+0x19C` | member constructor `0x35FB8` |
| `#groupDelayMax_INT` | `+0x1A0` | member constructor `0x35FBC` |
| `#initialHeading_INT` | `+0x1A4` | member/initial-motion heading |
| `#initialHeadingTolerance_INT` | `+0x1A8` | member/initial-motion jitter |
| `#appearsPercent_INT` | `+0x1C0` | group selection `0x369F0` |
| `#xOffsetMin_FLOAT` | `+0x25C` | initial position `0x37960` |
| `#xOffsetMax_FLOAT` | `+0x260` | initial position `0x37964` |
| `#yOffsetMin_FLOAT` | `+0x264` | initial position `0x37968` |
| `#yOffsetMax_FLOAT` | `+0x268` | initial position `0x3796C` |
| `#initialSpeedMin_FLOAT` | `+0x26C` | initial motion `0x37BBC` |
| `#initialSpeedMax_FLOAT` | `+0x270` | initial motion `0x37BC0` |
| draw-layer ID | `+0x2E0` | copied to live member `+0x4C` |

See `ENTITY_CONSTRUCTION.md` for the execution and RNG-order consequences.


## State owner-location anchors

The state parser around `0x40920` independently exposes the destination bytes
used by the post-construction owner-location routines:

| Serialized state key | Compiled state offset | Runtime consumer |
| --- | ---: | --- |
| `#stateLockToOwnerLoc_BOOL` | `+0x32E` | initializer `0x33600`, tick `0x37130` |
| `#stateLinkToOwnerLoc_BOOL` | `+0x32F` | initializer `0x33600`, tick `0x37230` |
| `#stateOrbitOwner_BOOL` | `+0x330` | initializer `0x33600`, tick `0x37350` |

Canonical 1.0.6 uses the three flags mutually exclusively: 156 Lock states,
10 Link states, and 8 Orbit states. See `ENTITY_WORLD_RUNTIME.md`.



## Visual/render anchors

Renderer wrapper `0x12F20`, state entry `0x146F0`, and the Unit/state loaders establish:

| Serialized Unit Definition key | Compiled offset | Runtime use |
| --- | ---: | --- |
| `#castsShadows_BOOL` | `+0x11E` | shadow eligibility |
| `#adjustShadowLocForScaling_BOOL` | `+0x12C` | shadow transform option |
| `#initialScalePercent_INT` | `+0x1AC` | initial scale percentage |
| `#initialScalePercentTolerance_INT` | `+0x1B0` | signed inclusive initial-scale RNG tolerance |
| `#initialVisibilityPercent_INT` | `+0x1B4` | initial live visibility |
| `#drawLayer_ID` | `+0x2E0` | main/shadow layer FourCC |

State base remains `UnitDef +0x4E0 + stateIndex*0x5E0`:

| Serialized state key | Compiled state offset |
| --- | ---: |
| `#stateSpriteFace_ID` | `+0x304` |
| `#stateSpriteFrameMin_INT` | `+0x30C` |
| `#stateSpriteFrameMax_INT` | `+0x310` |
| `#stateUseParentDirection_BOOL` | `+0x324` |
| `#stateTintColor_COLOR` | `+0x332` |
| `#stateDoColorise_BOOL` | `+0x34D` |
| `#stateDrawToTerrain_BOOL` | `+0x353` |
| `#stateRequiredScalePercent_INT` | `+0x3BC` |
| `#stateScaleDeltaPercent_INT` | `+0x3C0` |
| `#stateRequiredVisibilityPercent_INT` | `+0x3C4` |
| `#stateVisibilityDeltaPercent_INT` | `+0x3C8` |
| `#stateTintPercent_INT` | `+0x3CC` |
| `#stateTintDeltaPercent_INT` | `+0x3D0` |

See `RENDER_VISUAL_RUNTIME.md` for the shared live sprite base, exact scalar ramps,
layer mapping, and render-request order.

## Collision/damage anchors

Collision scan `0x36CF0`, damage routine `0x14F10`, and the Unit Definition
loader establish the following direct anchors. `UnitDef +0x08` is a derived
FourCC: loader code writes `grnd` when `isGroundBased_BOOL` is true and `air `
otherwise. It is therefore a collision domain, not the draw-layer ID at
`+0x2E0`.

| Serialized Unit Definition key | Compiled offset | Runtime use |
| --- | ---: | --- |
| `#harmlessToPlayers_BOOL` | `+0x11A` | candidate classes must be opposite |
| `#playerProjectile_BOOL` | `+0x11B` | projectile candidate/offscreen policy |
| `#canBeHitByPlayerProjectile_BOOL` | `+0x11C` | projectile compatibility |
| `#castsShadows_BOOL` | `+0x11E` | copied into live render state during obstacle conversion |
| `#hittableWhenInvisible_BOOL` | `+0x121` | live collision-participation/visibility byte |
| `#isGroundBased_BOOL` | `+0x125` | derives `UnitDef+0x08` = `grnd` / `air ` |
| `#collidesWithGroundObstacles_BOOL` | `+0x128` | persistent ground-obstacle Rect query |
| `#doDeathSpawnOnAnyMedia_BOOL` | `+0x12B` | bypasses `0x16880` Media Mask routing |
| `#damage_FLOAT` | `+0x274` | damage dealt in entity collision |
| `#hitParticles_ID` | `+0x2D8` | hit particle effect |
| `#mediaImpactSize_ID` | `+0x2E4` | `0x16880` water-impact replacement selector |
| `#shields_BaseAmount_FLOAT` | `+0x43C` | base shields |
| `#shields_LevelIncrement_FLOAT` | `+0x440` | level-dependent shield constructor term |
| `#shields_MaxAmount_FLOAT` | `+0x444` | shield constructor clamp |
| `#score_INT` | `+0x4B8` | shield-depletion score value |

State base remains `UnitDef +0x4E0 + stateIndex*0x5E0`. Collision-facing state
anchors are:

| Serialized state key | Compiled state offset |
| --- | ---: |
| `#collision_Spawn_ID` | `+0x2E0` |
| `#collision_RepeatSpawns_BOOL` | `+0x2E4` |
| `#collision_SpawnDelay_INT` | `+0x2E8` |
| `#passHitsToOwner_BOOL` | `+0x32B` |
| `#stateCollides_BOOL` | `+0x347` |
| `#stateInvulnerable_ShieldsDoNotDepleteOnCollision_BOOL` | `+0x348` |
| `#stateIsTargetable_BOOL` | `+0x34E` |
| `#stateCollidesWithPlayers_BOOL` | `+0x34F` |
| `#stateDoNotGlowOnCollision_BOOL` | `+0x354` |
| `#stateUseThisStateOnShieldDepletion_BOOL` | `+0x356` | constructor caches existence in live `+0xCD`; `0x17E70` enters first marked state |
| `#stateOnHitChangeStateDelay_INT` | `+0x3B8` |
| `#stateOnHitChangeTo_STR` | `+0x59C` |

See `COLLISION_DAMAGE_RUNTIME.md` for scan/damage behavior and `TERRAIN_MEDIA_RUNTIME.md` for the recovered media/ground-obstacle paths.

### Derived live-member caches

Two previously unnamed live bytes are now source-to-runtime proven:

- live `+0x19` = `(UnitDef+0x08 == 'air ')`, written at `0x35F88..0x35FA0`;
- live `+0xCD` = whether any compiled state `+0x356` is nonzero, written by the constructor scan `0x35DAC..0x35DF0`.

See `ENTITY_CORE_EDGE_RUNTIME.md` for the consumers and branch behavior.


## Destruction/removal anchors

PPC `0x16300`, `0x36120`, the Unit Definition loader, and the state loader now establish these direct fields:

| Serialized key | Compiled offset | Use |
| --- | ---: | --- |
| `#deletionSpawn_ID` | `+0x2DC` | deletion-only spawn in outer cleanup |
| `#destructSpawn_ID` | `+0x478` | ordinary destruction spawn |
| `#destructParticle_ID` | `+0x47C` | destruction particle resource |
| `#destructParticleColor_COLOR` | `+0x480` | packed particle color |
| `#destructNotice_STR` | `+0x482` | destruction notice |
| `#destructNumCoinsToRelease_INT` | `+0x4A4` | ordinary coin count |
| `#destructCoin_ID` | `+0x4A8` | ordinary coin resource |
| `#destructCoinOnGroupKill_ID` | `+0x4AC` | group-kill reward resource |
| `#destructDestroyChildren_BOOL` | `+0x4B0` | destroy opted-in children |
| `#destructDeleteChildren_BOOL` | `+0x4B1` | delete opted-in children |
| `#destructCreateObstacle_BOOL` | `+0x4B2` | obstacle conversion request |
| `#destructDrawToTerrain_BOOL` | `+0x4B3` | terrain draw request for ground unit |
| `#destructReleaseRandomBonus_BOOL` | `+0x4B4` | random-bonus selection |
| `#score_INT` | `+0x4B8` | score contribution |
| `#destructSound_ID` | `+0x4BC` | destruction sound ID |
| `#destructSound_MinVolume_INT` | `+0x4C0` | sound minimum volume |
| `#destructSound_MaxVolume_INT` | `+0x4C4` | sound maximum volume |
| `#destructSound_Priority_INT` | `+0x4C8` | sound priority |
| `#destructSound_MinPitch_FLOAT` | `+0x4CC` | sound minimum pitch |
| `#destructSound_MaxPitch_FLOAT` | `+0x4D0` | sound maximum pitch |
| `#pickup_Type_ID` | `+0x4D4` | player pickup discriminator |
| `#pickup_Value_INT` | `+0x4DC` | player pickup value |

State-relative destruction flags are `canBeDestroyedOnOwnerDestruction_BOOL +0x329`, `canBeDeletedOnOwnerDeletion_BOOL +0x32A`, `passHitsToOwner_BOOL +0x32B`, and `destroyOwnerOnDestruction_BOOL +0x32D`. See `DESTRUCTION_GROUP_RUNTIME.md` for ordering and group-counter semantics.
