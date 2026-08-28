# Level resource (`.leve`) format

Status: **confirmed for all 12 canonical 1.0.6 levels**.

After the legacy text byte transform is decoded, every level begins with the same eleven fields in the same order:

1. `name_STR`
2. `indentifier_STR` — historical misspelling preserved in source data
3. `description_STR`
4. `copyright_STR`
5. `background_RECT`
6. `backgroundImage_ID`
7. `previewImage_ID`
8. `music_ID`
9. `mediaMask_ID`
10. `briefing_ID`
11. `numObjects_INT`

The header is followed by exactly `numObjects_INT` placement records. Each placement contains seven fields:

1. `unit_ID`
2. `layer_ID`
3. `xLoc_INT`
4. `yLoc_INT`
5. `headingDegrees_INT`
6. `isStationary_BOOL`
7. `enableTerrainEffects_BOOL`

Across the canonical campaign, the declared counts reconcile exactly with **565 placed objects**.

All observed level backgrounds are `0, 0, 480, 3600`; all currently reference `mu03` for music and `none` for briefing. These are corpus observations, not hard-coded engine rules.

The clean `LevelDefinition` loader validates field order, typed values, and exact object-count consistency. See `reverse/inventories/LEVELS_1_0_6.json`.
