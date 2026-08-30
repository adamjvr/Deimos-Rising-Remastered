# Level placement activation runtime — Mac 1.0.6

Status: **binary-call-shape recovered, canonical-data corroborated, live-host implemented**.

## Recovered row stream

Terrain initialization `0xFA10` calls world routine `0x33090` once for every
world Y row from the visible source bottom through `sourceTop - 64`. Normal
terrain scrolling later calls that same routine for exactly the newly exposed
`sourceTop - 64` row. The live host previously ignored this boundary and
constructed every serialized Level placement at startup.

`LevelPlacementActivationRuntime` is the clean one-shot scheduler that maps
those world-row callbacks onto `LevelDefinition::objects`. It tracks activation
per serialized placement, emits all placements whose `y` equals the reached
row, and preserves source order for same-row placements.

For canonical Level 1 (`le01`, background `cam1`), the initial source rectangle
is Y `3120..3600`; the 64-pixel activation margin reaches Y `3056`. Of the 46
serialized placement groups, only Y `3309` and `3129` are reached initially.
The next placement at Y `3020` becomes eligible on tick 36 as the source top
moves upward one row per tick.

## Camera-delta rule

A placement activated by the current terrain callback is constructed with
`subtract_world_y_origin=true`, so its screen Y already reflects the newly
advanced source origin. Only members that existed before the terrain step are
shifted by the current camera delta. Applying the delta to a newly activated
member would move it twice.

## Evidence boundary

The call schedule into `0x33090` is recovered. The current exact-y placement
match is strongly corroborated by the serialized Level corpus and fixes the
observed encounter density, but the internals of `0x33090` are not yet fully
instruction-closed. The scheduler therefore lives in its own module and test,
rather than being represented as a solved internal PPC routine.

## Canonical witness

`deimos_original_frame_probe` requires:

- 2 initial activated placements / 2 initial live members;
- first later activation at tick 36;
- 3 placements activated by tick 120;
- unchanged initial, tick-1, tick-30, right-input, live-initial and live-fire frame hashes.
