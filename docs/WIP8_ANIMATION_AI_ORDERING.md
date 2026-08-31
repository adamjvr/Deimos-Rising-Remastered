# WIP8 animation, AI ordering, scroll-pause, and ground-placement audit

Status: **implemented and validation-frozen on 2026-08-30**.

WIP8 was started after the WIP7 playtest showed that enemies still behaved
incorrectly and some ground objects appeared visually misplaced. The pass did
not move serialized Level-1 coordinates by eye. It first closed shared runtime
semantics that can make a correctly placed asymmetric sprite look wrong.

## Recovered contracts implemented

### State animation / orientation

The live state compiler now carries the fields consumed by PPC `0x146F0` and
`0x15930`:

- `stateDoAnimateBackwards_BOOL`;
- `stateDoLoopAnimation_BOOL`;
- `stateContinuousFrameRandomisation_BOOL`;
- `stateDoRotateToTarget_BOOL`;
- `stateNumDirections_INT`;
- `stateSpriteFrameMin_INT` / `stateSpriteFrameMax_INT`;
- `stateFramesPerDirection_INT`;
- `stateFrameDelay_INT`;
- `stateFrameDelta_INT`;
- `stateUseParentDirection_BOOL`;
- `statePauseVerticalScrolling_BOOL`.

Each live member now keeps a sprite frame, directional index, last-animation
tick, and `animation_stopped` bit. State entry initializes the directional frame
from the physical/editor heading for directional states, while non-directional
static atlas selectors preserve serialized `stateSpriteFrameMin` (important for
`nosw` Shield Warning using `noti` frame 4 rather than the unrelated GET READY
frame 0).

The animation cadence is the recovered strict predicate:

```text
currentTick > lastAnimationTick + FrameDelay
```

Forward/reverse progression, loop wrapping, continuous randomisation, finite
stop, and directional subranges are handled in the clean entity runtime.
`animation_stopped` is sampled by the five-slot rule evaluator after animation
updates, allowing `This Entity's Animation Has Stopped` to transition in the
same entity tick.

### Heading and RotateToTarget

PPC `0x16230` heading-to-direction mapping is represented separately from
physical motion. Regression witnesses include:

- 24 directions, heading 105 degrees -> frame 7;
- 24 directions, heading 255 degrees -> frame 17.

PPC `0x172D0` RotateToTarget advances only the visual directional index one step
at animation cadence. It does **not** overwrite `heading_degrees`. Positive
shortest directional difference increments; negative difference decrements.

A Level-1 tick-120 audit initially looked suspicious because Flipper Mk2 also
showed frames such as 5 and 32. Those members were in `Move South, Spawn Flames`,
a 36-frame non-directional animation (`raso`), not the 36-direction Hunt state.
Hunt-state frames matched their target geometry (for example a down-left target
produced frame 23 while down/right targets clustered around frames 16..18).

### Delayed group members

The WIP7 gate always skipped after decrementing a positive group delay. PPC
`0x33A54..0x33A78` instead skips only when the *new* delay remains positive.
WIP8 now makes a `1 -> 0` member participate in the remainder of that same tick.
A focused regression freezes `2 -> 1` as skipped and `1 -> 0` as eligible.

### Main member ordering

WIP7 advanced visible-screen position before rule/target/motion work. WIP8 moves
that phase into the recovered entity-update boundary:

1. delayed-member gate;
2. state particle / timer work;
3. animation;
4. ordered rules;
5. target/Hunt and range work;
6. Hold/Cyclic/Flee/convergence;
7. screen movement plus shipped `0x12CA0` lifetime gate;
8. owner Lock/Link/Orbit;
9. spawn scheduling / request construction;
10. ground-obstacle stop;
11. later collision/removal outer passes.

The owner-before-spawn portion is independently established around `0x3401C`.
The later obstacle slot is established around `0x344F8..0x34578`.

### Ground-obstacle ordering

Obstacle overlap zeros velocity and latches `stationary=true`. WIP7 performed
that immediately after movement, before spawn scheduling. That could cause a
same-tick terrain-effect child to observe the wrong parent stationary state.
WIP8 constructs due spawn requests first, then applies the obstacle stop in the
later shipped slot.

### Vertical-scroll pause latch

`statePauseVerticalScrolling_BOOL` is OR'd across processed active members for
the current frame. That latch suppresses the outer terrain-scroll/placement-row
boundary on the **following** frame, matching the recovered outer-loop shape.
The latch does not consume camera progress or activate new level rows while held.

A no-input 5,000-tick Level-1 audit produces long scripted holds in the same
regions found during PPC research (roughly tick 519 onward, the 1310-era long
encounter director chain, and a later 4160-era hold). These are state-driven
encounter behavior, not evidence that the terrain camera is stuck.

## Ground-coordinate audit conclusion

No global Level-1 X/Y correction is justified.

The existing spawn-coordinate distinction is already binary-confirmed:

- serialized level placements enter construction with
  `subtract_world_y_origin=true`;
- ordinary relative child spawns begin at the parent x/y;
- unrotated absolute child spawns begin at `(0,0)` and add their serialized
  offsets;
- child-spawn requests leave the constructor's subtract-world-origin flag zero;
- rotation-adjusted offsets always use the parent as their base; canonical data
  contains no set combining rotation-adjustment and absolute-coordinate flags.

Therefore globally adding/subtracting the terrain origin to spawned ground
objects would erase a recovered executable distinction. Any remaining visual
placement defect must be proven for a specific Unit Definition/spawn set before
changing geometry.

## Oracle-shift investigation

WIP7's deliberate opening-lane crash oracle was dying@185 / respawn@266. WIP8
initially produced dying@171 / respawn@252, so the change was isolated before
acceptance rather than copied into the expected values.

Cross tests showed:

- WIP8 ordering with animation disabled and the corrected group-delay gate:
  dying@184 / respawn@265;
- animation disabled plus the old WIP7 delay gate: exactly dying@185 /
  respawn@266;
- full WIP8: dying@171 / respawn@252.

Thus the large trajectory/timing change comes from enabling the previously
missing animation/orientation + Animation-Stopped state behavior, while the
binary-confirmed group-delay correction accounts for the remaining one tick in
the no-animation comparison. The historical static preview hashes remain
bit-identical; only live-world witnesses were re-frozen.

## WIP8 accepted live witnesses

Static/external preview witnesses remain unchanged:

- initial: `0x9e8a7ec73b79b254`;
- tick 1: `0x44dede08075273f2`;
- tick 30: `0x51d4a7eec9b0beef`;
- right-input tick 1: `0x6fd5c94a64dcb0c8`.

Live-world witnesses after WIP8 are:

- live initial: `0xcd72678207b195b7`;
- first air-fire tick: `0x800f06651d29406a`;
- live tick 120: `0x267609db3ba6dbcc`;
- tick-120 resident members/groups: 15 / 9;
- tick-120 maximum active during the probe: 18;
- opening-lane crash: dying@171, respawn@252.

The playable external-data probe continues to require actual Plasma Bomb ground
damage (`bsde` 4.0 -> 3.6), normal/locked `pbta` reticle behavior, 15-tick Ion
Cannon charge activation and `icps` release, non-zero player effects/particles,
and a bounded 3,000-tick stress world.

## Validation policy

WIP8 does not claim that every remaining AI/input/audio edge is now
instruction-perfect. It freezes the specifically recovered animation,
orientation, delayed-member, pause-latch, movement/spawn/obstacle ordering, and
coordinate contracts above. Remaining mismatches should be closed with a
specific PPC/data witness rather than by retuning positions or timings by eye.
