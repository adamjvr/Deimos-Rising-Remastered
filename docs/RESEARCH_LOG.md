# Research Log

## 2026-08-27 — multi-build corpus intake and first full extraction

### Mac 1.0.6

`DR-EVID-002` was decompressed through StuffIt method 15 (Arsenic/BWT), including its resource fork. The resulting 81,788,928-byte self-mounting image is a Classic Macintosh HFS volume named `Deimos Rising 1.0.6`.

The HFS catalog was extracted with no recorded extent problems. The install contains the PowerPC PEF application, resource fork, registration application, HID bundle, Player Guide, local-override directory tree, and four game PAKs.

### PAK/resource model

The PAKs are ZIP archives. The application emits diagnostics naming `Data:Paks`, `Data:Local`, and `U_Pak.cc`, states that only ZIP files are supported in Paks, and that ZIP files are not supported in Local. This strongly establishes a loose-local-overrides + packaged-resource architecture.

Observed loose directories are:

`coli`, `film`, `flli`, `idli`, `im08`, `im16`, `leve`, `plde`, `pref`, `reli`, `soun`, `stli`, `tefo`, `unde`, `wede`.

PAK totals:

- Audio: 96 files — all `.IMA` (AIFC/IMA4).
- Game: 763 actual files — 386 `unde`, 248 GIF, 54 `tefo`, 38 TGA, 12 `leve`, 6 `idli`, 5 `stli`, 5 `wede`, 4 `film`, 2 `plde`, and singleton `coli`, `flli`, `reli`.
- Interface: 9 actual files — 7 TGA + 2 GIF.
- Music: 3 files — one `.aif` and two `.IMA`, all AIFC/IMA4.

The 12 canonical level resources are `[le01]` through `[le12]`. Four canonical demos are `[de01]` through `[de04]`.

### Asset plate semantics

Mod documentation points replacements at `Data/Local/im08`. Image pairs use names such as `Player 1 Blue IA[PL1B].gif` and `Player 1 Blue IC[pl1b].gif`. The original application diagnostic `Sprite color and alpha plates are not equal size` independently corroborates that these are paired color/alpha plates.

### PEF executable

The main PEF imports ten libraries: MathLib, QuickTimeLib, AppearanceLib, InternetConfigLib, DrawSprocketLib, InterfaceLib, SoundLib, UnicodeConverter, TextCommon, and InputSprocketLib, totaling 445 imported symbols.

The executable retains unusually rich diagnostic strings, original implementation filenames, and serialized-field names. These provide independent naming anchors for reconstructing systems including levels, films/replays, sprites/images, resource PAKs, level selection, input, and audio.

### Add-ons / Apple Bundle updater

The add-ons archive contains an Apple Bundle 1.0.6 updater. Its Read Me states that it updates Apple-bundled copies from v1.0.0 through v1.0.5 and is not intended for shareware copies. Reported changes include OS X window/cursor/HID behavior, windowed fades/dissolves, high-score speed, locked-volume handling, Panther compatibility, fullscreen transition handling, preferences location, and error reporting.

The add-on corpus also exposes ten declared 40,296-byte demo films, multiple player/sprite mods, MP3 soundtrack files, original theme-song archives, desktop art, and player-guide/reference imagery.
