# Entity Construction — Mac 1.0.6 PPC reconstruction

Status: **normal group/member construction path substantially recovered and implemented headlessly**.

Code offsets refer to the Mac 1.0.6 PEF code section. The clean implementation intentionally excludes original pointers/intrusive-list internals while preserving proven request fields, serial identities, RNG order, initial position/motion, state entry, and group delay.

## Primary routines

| Routine | Offset | Recovered role |
| --- | ---: | --- |
| top constructor | `0x33220` | resolve UnitDef, group selection, gates, group allocation, member construction |
| group member loop | `0x35BF0` | create one or N surviving members; first-member output reference |
| live member constructor | `0x35CD0` | identity/options/position/motion/state/group-delay initialization |
| group selection | `0x369F0` | group-size and appearance rolls |
| safe entity reference check | `0x36AB0` | pointer + serial + active-byte validation |
| initial position | `0x37930` | UnitDef offset/random-location placement |
| initial motion | `0x37B50` | speed, heading/target vector, initial velocity |
| inclusive integer RNG | `0x46580` | signed endpoint-preserving integer range |
| float RNG | `0x465E0` | single-precision LCG mapping |
| base LCG | `0x553E0` | 15-bit random result |

## Exact 44-byte construction request

The request consumed by `0x33220` is now mapped far enough to replace the earlier provisional owner-token model:

| Offset | Clean field | Evidence |
| ---: | --- | --- |
| `+0x00` | Unit FourCC | resolved through `0x3D2F0` when caller does not supply UnitDef |
| `+0x04` | X | copied to group `+0x9C` |
| `+0x08` | Y | copied or world-origin adjusted to group `+0xA0` |
| `+0x0C` | subtract world Y origin | invokes `0xFEC0`; request Y is `fctiwz` before subtracting origin |
| `+0x0D` | explicit heading present | chooses explicit-heading constructor path |
| `+0x10` | explicit heading | passed to member construction when `+0x0D` is set |
| `+0x14` | signed player-owner index | copied to live member `+0xD8`; `-1` means no owner |
| `+0x18` | editor/default request heading | copied to group `+0xB4`; used when UnitDef says initial heading is editor-set |
| `+0x1C` | stationary | group `+0xB8`, then live member `+0x13C` |
| `+0x1D` | terrain effects enabled | group `+0xB9`, then live member `+0x13D` |
| `+0x20/+0x24` | parent pointer + serial | copied to live `+0x140/+0x144` and validated by `0x36AB0` |
| `+0x28` | initial velocity multiplier | applied after initial velocity construction when not exactly `1.0f` |

The clean request uses a portable entity handle plus serial instead of an original host pointer.

## Top-level constructor ordering

The ordering at `0x33220` is RNG-significant:

1. resolve Unit Definition;
2. **select group and perform appearance rolls** (`0x369F0`);
3. `canBeSpawnedOnlyWhenPlayersActive` gate;
4. `doNotSpawnIfTypeAlreadyExists` gate;
5. reject when `activeLiveMembers + survivors > 1000`;
6. optionally delete existing same-type entities owned by the request's player;
7. create/initialize the group container;
8. construct the surviving live members in sequence.

Because step 2 precedes the gates, a request rejected for player activity can still advance the global LCG.

## Group selection (`0x369F0`)

`numInGroupMin/Max` chooses the candidate group size. A variable range consumes one inclusive integer draw.

`appearsPercent` is then applied independently to every candidate:

- `100`: always survives, no appearance RNG;
- `0`: always removed, no appearance RNG;
- otherwise: choose inclusive `0..100` and remove only when `roll > appearsPercent`.

The resulting probability is therefore `(appearsPercent + 1) / 101`, not exactly `P/100`.

## Normal group/container

The normal path allocates **188 bytes (`0xBC`)** at `0x33454`. Proven fields include:

| Offset | Meaning |
| ---: | --- |
| `+0x94` | group serial |
| `+0x98` | Unit FourCC |
| `+0x9C/+0xA0` | base X/Y |
| `+0xA4` | original member count established at construction |
| `+0xA8` | active member count; initialized equal to original count |
| `+0xAC` | destroyed-member count; initialized to zero |
| `+0xB0` | member collection |
| `+0xB4` | request editor heading |
| `+0xB8/+0xB9` | stationary / terrain-effects options |

Group serials use their own monotonically increasing global counter, separate from live-member serials.

If request `+0x0C` is enabled, base Y is:

```text
float( truncTowardZero(requestY) - worldYOriginInteger )
```

not a direct floating subtraction.

A rare single-member special-parent branch exists before normal group allocation. Its sentinel/object-role semantics are not yet named and are intentionally excluded from the headless normal-path API.

## Live member identity and inheritance

`0x35CD0` allocates a live member, inserts it into the group's member collection, and establishes:

- independent live-member serial (`+0x9C`);
- containing group serial (`+0xA0`);
- player-owner byte (`+0xD8`);
- parent safe reference (`+0x140/+0x144`);
- draw-layer value from UnitDef `+0x2E0`;
- request stationary/terrain options at `+0x13C/+0x13D`.

When the caller asks for an output reference, only the **first member** of a multi-member group is written back as pointer+serial. The clean runtime mirrors this as `first_member_reference`.

## Heading selection before position/motion

The top constructor selects a heading mode:

```text
if request.explicitHeadingPresent:
    headingMode = true
    suppliedHeading = request.explicitHeading
else:
    headingMode = UnitDef.initialHeadingSetInEditor
    suppliedHeading = request.editorHeading
```

At `0x35EB4`, when `headingMode` is true, the member constructor applies `initialHeadingTolerance / 2` jitter to the supplied value and wraps once into 0..359. This occurs **before initial position and before the stationary early-out in initial motion**, so a stationary editor/explicit-heading member can still consume a tolerance RNG draw.

When `headingMode` is false, the provisional member heading is `UnitDef.initialHeading` without an RNG draw. The motion routine may later apply tolerance when it chooses that default heading for velocity.

## Float RNG (`0x465E0`)

Equal endpoints return immediately and consume no RNG.

Otherwise the routine:

```text
low    = min(minimum, maximum)
span   = frsp(maximum - minimum)   // sign preserved
r      = RNG15()
scaled = fmuls(span, float(r))
frac   = fdivs(scaled, 32767.0f)
result = fadds(frac, low)
```

Reversed endpoints are therefore not normalized into a conventional range. The clean helper preserves the signed span.

## Initial member location (`0x37930`)

UnitDef fields:

- `xOffsetMin/Max` (`+0x25C/+0x260`)
- `yOffsetMin/Max` (`+0x264/+0x268`)
- `randomiseInitialLoc` (`+0x12D`)

### Both axes vary

The original first chooses an integer angle `0..359` and fetches `{sin(angle), cos(angle)}`.

If `randomiseInitialLoc` is true:

```text
radius = floatRNG(0, abs(xOffsetMax))
x = groupX + sin(angle) * radius
y = groupY + cos(angle) * radius
```

If false, minima are ignored and the two maxima are used independently:

```text
x = groupX + sin(angle) * abs(xOffsetMax)
y = groupY + cos(angle) * abs(yOffsetMax)
```

This asymmetry is original behavior, not a cleaned-up interpretation.

### One/zero varying axes

Each varying axis is handled independently:

1. numerically sort its float endpoints;
2. truncate each endpoint toward zero;
3. use inclusive **integer** RNG between those truncated values;
4. add the integer result to the group base coordinate.

Equal endpoints are added directly with no RNG.

This path explains canonical `Screw Mk 2[sc02]`, whose Y offsets are written as `100..0`: the constructor sorts them to integer `0..100` before selecting the offset.

## Initial motion (`0x37B50`)

Stationary members return before initial-speed RNG and receive zero velocity.

Otherwise initial speed comes from float RNG over `initialSpeedMin/Max` (`+0x26C/+0x270`). The heading/vector path is then selected:

1. request/editor heading mode -> use the already-jittered supplied heading;
2. `initiallyHuntsClosestPlayer` -> query a world/player target, normalize the delta, scale by speed;
3. Burst/Implode -> group-relative vector path;
4. otherwise optionally inherit parent heading when `useOwnerHeading` is true and a valid parent exists;
5. fallback to `initialHeading`, applying tolerance RNG here;
6. convert heading to velocity with the legacy 360-entry sin/cos tables.

The request's initial-velocity multiplier is applied after this velocity is formed.

Canonical 1.0.6 statistics:

- variable initial speed: **36** Unit Definitions;
- randomized initial location: **9**;
- initially hunts closest player: **1** (`Mine[mine]`);
- Burst: **0**;
- Implode: **0**;
- one reversed X/Y offset range: `Screw Mk 2[sc02]` Y `100..0`.

The clean normal-path constructor is therefore complete for all canonical initial-motion branches except that the world-layer choice of *which* player Mine hunts remains an explicit caller-supplied fact. Burst/Implode remain preserved fields and are not falsely declared complete because canonical 1.0.6 does not exercise them.

## State entry and cumulative group delay

After position/motion, `0x35CD0` enters the initial state and initializes its spawn records. Only then does it choose `groupDelayMin..Max`.

Every member, including the first and final one, receives a draw when the endpoints differ. Delays accumulate across the group:

```text
member0.delay = draw0
member1.delay = draw0 + draw1
member2.delay = draw0 + draw1 + draw2
```

The cumulative value is stored at live-member `+0xB0`.

## Clean-core boundary

`construct_entity_group_headless()` now models the proven **normal** path through:

```text
group selection/gates
 -> group serial/base
 -> member serial/parent/owner
 -> pre-motion heading jitter
 -> initial location
 -> initial speed/vector
 -> state zero / immediate counter transitions
 -> state spawn-runtime initialization
 -> cumulative group delay
```

Still intentionally outside this boundary:

- intrusive world/group/member list implementation;
- the rare special single-member parent-container path;
- shield/render/animation fields that do not affect the recovered constructor RNG order;
- exact world routine used to select Mine's initial hunt target;
- Burst/Implode angle conversion compatibility path;
- later tracking/movement/collision/damage logic.
