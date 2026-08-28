# Status

## 2026-08-28 — Phase 1 data/resource reconstruction

### Evidence corpus

- `DR-EVID-001` — older StuffIt-packaged disc image; remains open for cross-version extraction/correlation.
- `DR-EVID-002` — Mac 1.0.6 installation fully recovered from its StuffIt/HFS layers.
- `DR-EVID-003` — Windows PE32/NSIS distribution identified; payload expansion and Mac↔Windows binary correlation remain active.
- `DR-EVID-004` — add-ons/update/reference/mod/music corpus; Apple Bundle update and Perfect Demos evidence recovered.

Phase 0 remains open only because more evidence may arrive. Active engineering is Phase 1.

### Clean core now implemented

- exact four-byte resource IDs and IA/IC plate-name parsing;
- dependency-free reader for the canonical stored-ZIP PAK subset;
- CRC32 validation for original PAK members;
- `Data/Local`-over-PAK resource provider;
- exact legacy seven-bit resource-byte decoder plus canonical synthetic encoder;
- generic tagged-text grammar parser and typed integer/float/Boolean/FourCC/RECT/RGB helpers;
- strict typed `.leve` loader;
- typed ID/float/color/rect/string list loaders and the 17-field Text Format loader;
- partial/proven-field `.film` v10005 parser;
- `deimos_reference_probe` for validating a user-owned original `Game.pak` directly through the clean core.

### Real-corpus validation

The clean C++ reader/parser has been run against the recovered original 1.0.6 resources, not only synthetic fixtures:

- `Audio.pak`: 96 actual files CRC-validated;
- `Game.pak`: 763 actual files CRC-validated;
- `Interface.pak`: 9 actual files CRC-validated;
- `Music.pak`: 3 actual files CRC-validated;
- **871 original files total**;
- all 12 canonical levels parsed;
- all **565** declared level placements reconciled;
- all 4 canonical PAK replay films parsed as v10005.

The repository test suite contains only synthetic clean fixtures and currently passes **7/7** tests.

### Major reverse-engineering findings

- Ten game-data extensions (`leve`, `unde`, `plde`, `wede`, `idli`, `flli`, `coli`, `tefo`, `stli`, `reli`) share a reversible seven-bit text transform rather than opaque binary serialization.
- All **473** canonical resources in those buckets decode cleanly to ASCII.
- Unit definitions reveal a heavily data-driven entity/state-machine model: 386 units, 1,167 repeated state records, and 5,835 state-rule records by canonical key counts.
- `.flli` is the global float/constants table; `.tefo` is Text Format; `.reli` is Rect List.
- Level format is fully mapped at the placement level.
- Fifteen v10005 films (4 canonical + 11 Perfect Demos) expose 135,840 active input ticks. The action set is known from documentation, but exact bit/action mapping remains intentionally unassigned until stronger evidence.
- Canonical level terrain geometry is 480×3600 with a corresponding 96×720 media mask, a 5:1 dimension ratio on each axis.

### Active reverse-engineering fronts

1. Type the `.unde`, `.wede`, `.plde`, and table resources without guessing uncertain behavioral semantics.
2. Map v10005 replay bit assignments and second-player record semantics from PPC/controlled evidence.
3. Reconstruct deterministic entity/state-machine execution from data + `G_*` / `U_*` code correspondences.
4. Expand and correlate the Windows build.
5. Continue PEF function/TOC/relocation mapping and bind binary behavior back to the clean simulation.
