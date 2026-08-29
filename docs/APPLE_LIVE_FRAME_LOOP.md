# Apple Original-Data Live Frame Loop

## Milestone

The native Apple host now advances the first deterministic, original-data-backed
Deimos runtime session instead of presenting only one static integration frame.
The loop remains intentionally narrower than the final game loop: it advances
only recovered subsystems whose outer semantics are already closed and tested.

```text
Game.pak + Interface.pak (user-owned, external)
        |
        v
OriginalGameFramePreview persistent session
        |
        +-- canonical FPS_MaxRate = 30
        +-- tick_legacy_terrain_scroll()
        +-- advance_legacy_score_bar_player()
        +-- persistent terrain/source/display surfaces
        +-- recovered gameplay-frame composition
        v
640x480 xRGB1555
        |
        v
AppleMetalHostView -> Metal -> macOS/iPadOS
```

## Deterministic frame oracles

The user's canonical 1.0.6 corpus establishes these complete 640x480 xRGB1555
FNV64 witnesses for Level 1 / Player 1:

```text
initial frame : 0x9e8a7ec73b79b254
tick 1        : 0x44dede08075273f2
tick 30       : 0x51d4a7eec9b0beef
FPS_MaxRate   : 30
```

`deimos_original_frame_probe` now requires all three hashes. This freezes not
only static asset decode/composition but also the first and thirtieth recovered
terrain/HUD ticks.

## Persistent state

Unlike the earlier static preview, the runtime now preserves:

- the full 480x3600 terrain surface;
- terrain camera/source-rectangle state;
- score-bar cache/meter state;
- the 416x480 game surface;
- the 576x480 game+HUD presentation source;
- the 640x480 canonical display surface;
- render/tick sequence counters.

The first frame seeds the static score-bar panel. Later ticks use the recovered
score-bar updater to mark only changing meter/text classes dirty.

The render queue is deliberately cleared and rebuilt for this fixture each
visible frame. Original layers 2..15 use resident queue records; the clean core
does not yet expose the full entity-owned record-update lifecycle, so retaining
and repeatedly appending the Player-1 preview request would be incorrect.

## Native cadence

The Apple host reads `FPS_MaxRate` from canonical `Game[gafl]` and schedules the
live preview at 30 Hz:

- macOS: main-run-loop `NSTimer`;
- iPadOS: `CADisplayLink` with preferred 30 FPS.

This cadence drives one deterministic recovered tick followed by one canonical
frame render/present. Input, entity spawning, collision, weapons, audio and the
full player lifecycle are not fabricated here; they remain the next integration
front.

## Next integration

The next step is to replace the fixture-only Player-1 render submission with the
real player/world orchestration and then bind native keyboard/controller/touch
input into the already-recovered player movement/action boundaries. After that,
entity and weapon ticks can join the same fixed 30 Hz session.
