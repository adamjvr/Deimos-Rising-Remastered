# Status

## 2026-08-27 — Phase 0 corpus intake + Phase 1 entry

### Evidence corpus

- `DR-EVID-001` — StuffIt-packaged disc image, externally associated with v1.0.3; internal version confirmation still pending.
- `DR-EVID-002` — Mac `Deimos Rising 1.0.6.smi`, fully decompressed and its HFS volume fully cataloged/extracted locally.
- `DR-EVID-003` — Windows PE32/NSIS distribution identified and installer structure mapped; payload expansion/correlation is pending.
- `DR-EVID-004` — add-ons corpus unpacked at the outer layer; official 1.0.6 Apple Bundle update extracted, plus guides, demo films, reference art, music, desktops, and community mods cataloged.

### Major reconstruction findings

- The Mac 1.0.6 image contains a complete installed game.
- Main game executable: PowerPC PEF, 2,045,976-byte data fork + 151,602-byte resource fork.
- Four canonical PAKs recovered: `Audio.pak`, `Game.pak`, `Interface.pak`, and `Music.pak`.
- PAKs are ordinary ZIP archives; their members are stored rather than deflated.
- `Game.pak` contains 763 actual files, including all 12 levels, 386 unit definitions, 248 GIF resources, 38 TGA resources, 54 terrain/formation resources, 5 weapon definitions, 2 player definitions, 6 ID lists, and 4 built-in replay films.
- `Audio.pak` contains 96 AIFC/IMA4 sound resources.
- `Music.pak` contains three AIFC/IMA4 music resources.
- Filename tags and application diagnostics establish a semantic four-character resource namespace and IA/IC alpha/color sprite-plate pairing.
- Application strings expose original subsystem/source names such as `G_Level.cc`, `G_Film.cc`, `U_Sprite.cc`, `M_Image.cc`, and `U_Pak.cc`, plus numerous serialized field names/assertions.
- The first portable clean implementation component now parses and preserves that recovered resource namespace independently.

### Active reverse-engineering fronts

1. Decode the custom binary serialization shared by `.leve`, `.unde`, `.plde`, `.wede`, `.idli`, `.flli`, `.coli`, and related resources.
2. Expand and correlate the Windows NSIS build.
3. Unpack PEF initialized-pattern data, relocations, TOC/import glue, and PowerPC code into a symbol/correspondence map.
4. Convert the 4 built-in + 10 add-on replay films into deterministic behavior regression tests.
5. Build the original-asset runtime provider with `Data/Local` override precedence over PAK resources.
