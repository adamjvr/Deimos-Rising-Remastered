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
- Relocated startup code references embedded build identity `1.0.6`, `Jan  2 2004`, `11:55:01`; the version is therefore internally confirmed from the executable.

## PEF loader summary

- 3 sections: code, pattern-initialized data, loader.
- 445 imported symbols across 10 libraries.
- Pattern-initialized data and relocation program are decoded.
- Main transition vector resolves to code offset `0x4D540` with r2/TOC at section-1 offset `0x8000`.
- 5,153 relocation fixups are reconstructed, including all 445 imports.
- Rich source/assert strings provide subsystem names and serialized-key candidates.

## Immediate use

This is now the primary canonical Mac data/art/audio corpus for reconstruction. DR-EVID-001 remains valuable as an older-build/version-difference oracle.
