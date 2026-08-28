# Deimos Rising Remastered

Clean, evidence-driven reconstruction/remaster project for **Deimos Rising**.

This repository is intentionally split between:

- `reverse/` — independently recorded observations, format notes, correspondence tables, and confidence-tagged reconstruction work.
- future `src/` / `include/` — maintainable clean implementation.
- `evidence/` — **manifests only** in Git. Original game archives, disc images, executables, and proprietary assets stay outside the repository.

## Phase 0 status

Evidence intake has begun with `DR-EVID-001`, the user-supplied `deimos-rising-install.sit`.

The StuffIt 5 catalog contains:

- `AboutThis.txt` (data + resource forks)
- `deimos_rising.iso` — 104,851,456 bytes uncompressed
- `Macintosh-Garden.txt`
- `MD5-checksum.txt`

The ISO is compressed with StuffIt method 13 and is the next extraction target.

## Project direction

Final target platforms:

- macOS
- iPadOS
- Linux
- Windows

Initial implementation priority is macOS/iPadOS plus a portable core, while keeping Linux and Windows first-class final targets.

See `docs/ROADMAP.md`.
