# Deimos Rising Remastered

Evidence-driven, clean-code reconstruction and native remaster of **Deimos Rising**.

## Evidence baseline

The working corpus includes an older Mac distribution, a fully recovered Mac **1.0.6** installation, a Windows PE32/NSIS distribution, and an add-ons/update/reference/mod/music corpus. Phase 0 remains open for additional evidence, while active development is in Phase 1 with the first binary-confirmed Phase-2 state-runtime primitives now implemented.

## Reconstruction rule

**Game executable logic is independently reconstructed. Original supplied art/audio/data are intentionally used as the canonical asset baseline until restored/upscaled replacements are produced.** Exact resource identities remain stable so restored content can override originals one asset at a time. See `docs/CLEAN_ROOM.md` and `docs/ASSET_POLICY.md`.

## Clean core status

The portable C++20 core now contains:

- exact four-byte resource/FourCC naming;
- IA/IC alpha/color plate parsing;
- a dependency-free reader for the original stored-ZIP PAKs with CRC32 validation;
- `Data/Local`-over-PAK lookup;
- the recovered legacy seven-bit tagged-text decoder/parser;
- typed scalar/FourCC/RECT/RGB helpers;
- a strict level loader validated against all 12 original levels;
- typed ID/float/color/rect/string-table and Text Format loaders;
- a conservative v10005 replay parser for fields proven from the corpus;
- typed unit/weapon/player definitions with explicit states, spawn sets, rules, and weapon spawns;
- a unit-behavior compiler with the complete 17-condition PPC dispatch vocabulary;
- exact first-match rule execution and exact/case-sensitive state-action resolution;
- binary-confirmed timer, state-entry-counter, and range-transition runtime primitives;
- binary-confirmed spawn scheduler, target terrain-effect gate, rotated/absolute/relative geometry, and full constructor-request handoff;
- headless normal group/member construction with original group/appearance RNG, owner/parent identity, serials, initial position/speed/heading, state-zero entry, spawn-runtime initialization, and cumulative group delay;
- recovered original 360-entry trig-table construction contract;
- cross-resource reference validation;
- a PEF packed-data/import/relocation probe that resolves the original main transition vector and TOC;
- a `deimos_reference_probe` executable that validates an original `Game.pak` through the clean implementation.

The original 1.0.6 `Game.pak` has been loaded directly through this clean code: all 763 files CRC-validate, all 12 levels parse, all 565 level placements reconcile, and all four canonical PAK films parse, and all 386 unit definitions / 5 weapons / 2 player definitions parse and cross-validate. Across all four original PAKs, **871 actual files** pass CRC validation.

## Build tests

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build -j
ctest --test-dir build --output-on-failure

# Optional: validate your local canonical reference PAK directly
./build/deimos_reference_probe reference/DR-EVID-002/canonical/Paks/Game.pak
```

Committed tests use synthetic fixtures only; the current suite passes 18/18. Original assets remain in the ignored local reference workspace.

See `docs/STATUS.md`, `docs/ROADMAP.md`, and `reverse/formats/` for current reconstruction details.
