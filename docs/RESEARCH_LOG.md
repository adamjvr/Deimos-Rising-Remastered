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

## 2026-08-28 — tagged-text, PAK runtime, level, and film breakthrough

### Legacy data transform

The apparent binary resources were decoded as seven-bit ASCII under a reversible byte transform. A corpus pass decoded all 473 canonical `leve/unde/plde/wede/idli/flli/coli/tefo/stli/reli` resources without replacement characters. The clean core now implements the exact decoder and a deliberately non-historical canonical inverse for synthetic fixtures.

The transform also exposed corrected identities for previously tentative buckets: `flli` is Float List, `tefo` is Text Format, `stli` is String List, and `reli` is Rect List.

### Data-driven game model

The 386 unit definitions contain extensive declarative state-machine, movement, sprite, sound, spawn, shield/damage, and rule data. Repeated-key counts identify 1,167 state records and 5,835 state-rule records across the canonical corpus. This shifts the clean-room strategy toward reconstructing the state-machine interpreter rather than hard-coding 386 entity classes.

### Levels

All 12 levels share one strict 11-field header followed by seven-field object placements. The 565 declared object placements reconcile exactly. A typed clean C++ loader now validates the original levels.

### Replay films

The Perfect Demos BinHex/StuffIt material yielded eleven additional 40,296-byte v10005 replay films. Combined with the four canonical PAK demos, 15 recordings provide 135,840 active input ticks. Each tick carries a seven-bit mask; two bit pairs have opposing-direction statistical signatures and a rare seventh bit is consistent with documented weapon switching, but exact action names remain unassigned pending stronger executable/two-player evidence.

### Runtime resource access

All four canonical PAKs use only ZIP method 0/stored members. A dependency-free clean PAK reader with CRC32 validation and a Local-over-PAK `ResourceStore` was implemented. It successfully CRC-validated all 871 actual original files and parsed all original levels/replays directly from `Game.pak`.

### Text-format type correction

Real-corpus validation showed that `tefo` field `Format_ID` is polymorphic at the text level: values include the four-character tokens `LEFT`, `CENT`, `CEBU`, `RIGHT` and the one-character values `3` and `4`. The clean parser preserves this as an opaque token rather than assuming the `_ID` suffix implies a FourCC.
