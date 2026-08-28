# Clean-room Boundary

## New code

Game logic, parsers, simulation, platform integration, and tools that become part of the remaster are independently written from observed behavior, file structures, resource identities, public platform/API documentation, and recorded tests. Do not copy decompiler pseudocode or machine-translated original implementation into the clean source tree.

## Original game assets

Original supplied art, audio, UI, levels, and other data are **reference/canonical content**, not forbidden by the code clean-room rule. They are intentionally used during reconstruction and as the initial remaster asset tier until restored/upscaled replacements are made.

Bulk proprietary evidence remains outside Git by default under `reference/`. This repository records hashes, manifests, semantic IDs, format descriptions, tests, and clean implementation code. A developer with the supplied original data can populate the local reference asset tree.

## Allowed observations

- hashes, sizes, offsets, resource tags, filenames, and paths;
- binary/container structure and independently described data fields;
- behavior measurements and replay outputs;
- API/library imports and linkage structure;
- original source filenames/assertion text as correspondence evidence;
- manually assigned semantic names supported by evidence;
- original assets in the local runtime/reference tree;
- synthetic fixtures and clean tests.

## Confidence labels

Use `confirmed`, `strong`, `tentative`, and `unknown`. Unknown fields remain `fieldN`/`unknown_*` until evidence supports a semantic name.
