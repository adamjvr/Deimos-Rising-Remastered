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
