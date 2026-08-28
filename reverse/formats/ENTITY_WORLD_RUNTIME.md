# Entity world / owner-location runtime — Mac 1.0.6

Status: **binary-confirmed normal world-query and owner-location subset**.

This document covers the first post-construction world layer recovered from the
1.0.6 PowerPC executable. It deliberately does not describe collision, damage,
scoring, death effects, or the still-unmapped intrusive-list allocator details.

## Safe live-member reference — `0x36AB0`

The original does not trust a parent pointer by itself. A reference consists of
an object pointer plus the serial captured when the reference was made.

A reference is valid only when:

1. the pointer is non-null;
2. the stored serial equals live-member `+0x9C`;
3. live-member byte `+0xCB` is zero (member is still active).

The clean world substitutes a stable numeric `EntityHandle` for the raw pointer
while retaining the same **handle + serial + active lifecycle** contract.

## World duplicate query — `0x36AF0`

`0x36AF0` walks all group/member lists and returns the first active live member
whose compiled Unit Definition ID equals the requested FourCC.

This is the world query used by:

`#doNotSpawnIfTypeAlreadyExists_BOOL`

The clean `EntityWorld::find_first_active_unit()` / `has_active_unit()` mirror
the proven observable behavior without reproducing the original intrusive list.

## Delete-existing-owned-type scan — `0x36BE0`

The routine walks active members and selects entries where:

- Unit ID equals the requested Unit FourCC; and
- signed live-member player-owner byte `+0xD8` equals the request owner index.

Each match is passed into the original member-removal path `0x36120`.

The destruction/removal semantics inside `0x36120` are now reconstructed; see `DESTRUCTION_GROUP_RUNTIME.md` for child propagation, group counters, rewards, and cleanup ordering.
The clean world therefore implements only the proven world-visible consequence
for constructor use: matching members cease to participate as active entities.
It does **not** invent death particles, scoring, or death-spawn behavior.

## Owner-location state flags

The Unit Definition state parser around `0x40920` ties the serialized keys
directly to compiled-state bytes:

| Serialized key | Compiled state offset |
| --- | ---: |
| `#stateLockToOwnerLoc_BOOL` | `+0x32E` |
| `#stateLinkToOwnerLoc_BOOL` | `+0x32F` |
| `#stateOrbitOwner_BOOL` | `+0x330` |

Canonical 1.0.6 uses these mutually exclusively:

- **156 Lock states across 71 units**;
- **10 Link states across 5 units**;
- **8 Orbit states across 4 units**.

## Owner resolution priority

Initializer `0x33600` and all three update routines resolve the owner in the
same order:

1. validate live `+0x140/+0x144` as a safe parent reference with `0x36AB0`;
2. if valid, use the parent live-member position;
3. otherwise, if signed player-owner index `+0xD8 != -1`, query that player
   position through `0x6090`;
4. otherwise no owner position is available.

A stale parent reference therefore falls through to the player owner instead of
blocking it.

## State-entry initializer — `0x33600`

`0x33600` is called both:

- during live-member construction at `0x35FB4`; and
- after ordinary state changes at `0x14D74`.

It begins by zeroing:

| Live-member offset | Recovered use |
| ---: | --- |
| `+0x124` | owner-relative / owner-mode X bookkeeping |
| `+0x128` | owner-relative / owner-mode Y bookkeeping |
| `+0x12C` | previous owner X |
| `+0x130` | previous owner Y |

If no Lock/Link/Orbit flag is set, it returns.

If an owner position resolves, it stores that position to `+0x12C/+0x130`.

### Lock initialization

For `stateLockToOwnerLoc`, `+0x124/+0x128` become the signed current
member-minus-owner offset. This fixed offset is used by `0x37130` on every
subsequent tick.

### Link initialization

Link needs only the previous owner position at `+0x12C/+0x130`. The child is
later translated by the amount the owner moved since the previous Link phase.

### Orbit initialization

Orbit stores:

- member-minus-owner offset in `+0x124/+0x128`;
- an integer-truncated radius as float at `+0xDC`;
- an integer angle at `+0xE0` from `0x42AD0 -> 0x43090`.

The distance helper `0x42E90` itself integer-gates the squared distance before
sqrt; `0x33600` truncates the resulting distance again before storing it.

## Lock update — `0x37130`

When a current owner resolves:

`member = owner + fixedOffset`

The fixed offset is the value captured by `0x33600` when the Lock state was
entered. Lock therefore rigidly preserves the relative location while the
owner moves.

## Link update — `0x37230`

Let `previousOwner` be `+0x12C/+0x130` and `currentOwner` the newly resolved
position. The original computes:

`member += currentOwner - previousOwner`

then stores `currentOwner` as the next previous-owner position.

Thus Link transfers only the owner's translation; the child can still move
independently between Link phases.

## Orbit update — `0x37350`

Orbit first resolves the owner and current member position. If they are equal,
it returns immediately.

If stored orbit radius `+0xDC` is non-zero, the routine reads **live-member
`+0x10`**, truncates it to an integer, and uses that integer as the angular
step. Independent initial-motion disassembly at `0x37B50` proves live
`+0x10/+0x14` are velocity X/Y, so this unusual reuse of velocity X is literal
original behavior.

When the truncated step is non-zero:

1. add the step to orbit angle `+0xE0`;
2. wrap once into `0..359`;
3. call `0x42B80(angle, radius)`;
4. set member position to `owner + returnedVector`.

With zero step or zero radius, the routine uses `owner + storedOffset` instead.
It finishes by refreshing `+0x124/+0x128` from the resulting member-owner delta.

## Legacy atan table — startup `0x42970..0x429BC`

The integer angle helper is not a direct `atan2` call. Startup generates a
1,024-entry integer table:

`table[i] = trunc(MathLib::atan(i * 0.01) * 57.2957795)`

The embedded double constants are:

- `0.01` — bits `0x3F847AE147AE147B`;
- `57.2957795` — bits `0x404CA5DC1A47A9E3`;
- lookup index scale `100.0` — bits `0x4059000000000000`.

`0x43090` selects a ratio, indexes the table, applies the original quadrant
transform, subtracts 90 degrees, and wraps the result. The clean math layer now
reproduces this generated-table contract rather than using host `atan2`.

As with the previously recovered sin/cos tables, the final transcendental call
currently uses the host C++ math library after preserving the original input
constants and truncation points. Replay comparison can later establish whether
classic MathLib produces any boundary-entry differences.

## Per-member ordering

The main update around `0x3401C` establishes:

1. `stateLockToOwnerLoc` -> `0x37130`;
2. `stateLinkToOwnerLoc` -> `0x37230`;
3. `stateOrbitOwner` -> `0x37350`;
4. spawn scheduler -> `0x15B40`.

These owner modes occur **after the previously recovered range-transition
phase and before spawn scheduling**. The clean world-aware tick installs the
owner phase at that exact boundary.

## Clean implementation boundary

`EntityWorld` currently provides:

- live group/member registration;
- handle lookup;
- safe handle+serial resolution;
- active member count;
- first-active-Unit-ID query;
- owned-type deletion marking;
- parent-first/player-fallback owner resolution;
- Lock/Link/Orbit initialization and update;
- a world-aware wrapper around the recovered entity tick.

Still separate:

- original intrusive-list node layout and allocator pools;
- player subsystem internals behind `0x6090`;
- downstream player life decrement/respawn/game-over state after the now-concrete pickup/shield/death-entry layer;
- integrate the proven zero-velocity/stationary ground-obstacle stop around its still-bounded live `+0x19` gate, renderer effects beyond the recovered `0x16880` media route/Rect store, plus special destruction branch `0x17E70`;
- target-selection/hunting world queries;
- ground/terrain collision and remaining scoring/game-state integration.
