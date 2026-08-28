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
