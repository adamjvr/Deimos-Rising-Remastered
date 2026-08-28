# Clean-room / Repository Boundary

## Allowed in Git

- hashes and manifests;
- independently written parsers;
- observed structure descriptions;
- offsets, sizes, IDs, resource names, and behavioral measurements;
- correspondence tables;
- manually assigned semantic names supported by evidence;
- synthetic fixtures and regression tests;
- clean implementation.

## Kept outside Git

- original StuffIt archives;
- extracted ISOs/disc images;
- original executables;
- original proprietary graphics/audio/data;
- bulk decompiler output;
- copied implementation code from the original binary.

Use `fieldN`, `unknown_*`, or `*_candidate` until semantics are supported by evidence.
