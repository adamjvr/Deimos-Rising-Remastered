# Gameplay frame orchestration — Deimos Rising 1.0.6

## Scope

This note binds the reconstructed score-bar pixel producer, world compositor,
and mode-1 presenter into the first complete portable normal-gameplay frame
boundary.

The ordering is not architectural guesswork. The original gameplay loop around
`0x58F0..0x5AB4` provides the missing outer evidence:

```text
0x5A18 -> 0x7070
              -> 0x31AE0 score-bar dirty draw
...
0x5AB0 -> 0x30570
              -> 0x30BC0 world frame + post-world presentation
```

Thus score-bar drawing precedes the frame-object world/presentation call.

## Recovered visible-frame order

The clean `render_legacy_gameplay_frame()` executes:

```text
score-bar static panel seed (session/reset only)
        |
score-bar P1/P2 dirty pixel draws
        |
0x30BC0 world composition
   group 0
   terrain viewport
   group 1
   particles
   group 2
        |
copy completed 416x480 game into source x=0..415
        |
source canvas = 576x480
   game      x=0..415
   score bar x=416..575
        |
mode-1 presentation plan (0xBEB0 contract)
        |
640x480 minimum frame
32 + 416 + 160 + 32
        |
legacy immediate CWindow CopyBits / no DSp swap
```

The score-bar updater itself remains simulation-side. Original `0x317E0` runs
before `0x7070`; the frame orchestrator consumes already-advanced score-bar
state and only performs the recovered visible draw order.

## Why game and score-bar surfaces remain separable

The portable implementation renders the world into an exact 416x480 game
surface and keeps the 576x480 presentation source as a persistent composition
surface. After world completion it copies only X `0..415` into that source.
This guarantees that persistent score-bar pixels at X `416..575` cannot be
clobbered by world clipping differences.

This clean separation is compatible with either interpretation of the legacy
GWorld internals while preserving the observable `0xBEB0` source geometry.

## Gates

Two original gates remain independent:

- `world_draw_enabled` maps the recovered `0x30BC0` world draw latch, gating
  terrain viewport copy and particles while sprite groups still flush;
- `presentation_enabled` maps the separate post-world presenter gate.

When presentation is disabled, the clean source frame is still produced; only
the final display copy is suppressed.

## Clean implementation

Files:

- `include/deimos/gameplay_frame_runtime.hpp`;
- `src/core/gameplay_frame_runtime.cpp`;
- `tests/gameplay_frame_runtime_test.cpp`.

The regression freezes:

- static panel seed before world composition;
- preservation of score-bar X `416..575`;
- complete world replacement of source X `0..415`;
- normal gameplay mode byte `1`;
- exact 32/416/160/32 display placement;
- no-swap historical commit classification;
- presentation-gate behavior with source production still active.
