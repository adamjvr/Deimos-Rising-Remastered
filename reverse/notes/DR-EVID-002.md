# DR-EVID-002 — Mac 1.0.6

## Confirmed

- StuffIt 5 archive containing `Deimos Rising 1.0.6.smi`.
- SMI data fork decompresses to 81,788,928 bytes; resource fork to 119,292 bytes.
- SMI is an HFS volume named `Deimos Rising 1.0.6`.
- Complete game installation recovered.
- Main `Deimos Rising` executable is PowerPC PEF, with a 2,045,976-byte data fork and 151,602-byte resource fork.
- Four PAKs recovered and opened as standard ZIP archives.
- All twelve numbered level definitions are present.
- `Game.pak` has 386 `.unde` resources and four built-in `.film` recordings.
- Audio `.IMA` assets are AIFC/IMA4.

## PEF loader summary

- 3 sections: code, pattern-initialized data, loader.
- 445 imported symbols across 10 libraries.
- Entry descriptor points into initialized data and will require normal PEF TOC/entry reconstruction.
- Rich source/assert strings provide subsystem names and serialized-key candidates.

## Immediate use

This is now the primary canonical Mac data/art/audio corpus for reconstruction. DR-EVID-001 remains valuable as an older-build/version-difference oracle.
