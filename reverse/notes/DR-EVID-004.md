# DR-EVID-004 — add-ons/update corpus

## Classes

### Official/update/reference

- Apple Bundle 1.0.6 updater
- Player guide
- Deimos Rising image/reference archive
- demo-level/replay archive
- desktop art
- perfect-demo BinHex/StuffIt archive
- soundtrack/theme-song material

### Community modifications

- 2nd Player
- FunnyShips
- Mario Rising
- StarCraft Battlecruiser player replacement
- player mod
- SillySprites 0.2

Mods are valuable evidence of resource override conventions, but they are not canonical gameplay/assets.

## Important format lead

Mod instructions target loose graphics in `Data/Local/im08`. Their filenames use the same four-character tags and IA/IC pairing visible in the original resources. This independently demonstrates that the engine deliberately supported replacement resources through the Local hierarchy.

## Regression corpus

`Demo_Levels.sit` declares ten replay films (`[de01]` through `[de10]`), each 40,296 bytes uncompressed. These should become simulation-regression inputs once `.film` is decoded.
