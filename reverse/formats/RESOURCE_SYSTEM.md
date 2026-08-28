# Resource system — reconstructed contract

Status: **confirmed core behavior / strong remaining details**.

## Providers

The original install contains packaged resources under `Data/Paks/*.pak` and loose override/category directories under `Data/Local/*`. Application diagnostics name both roots, identify ZIP as the PAK representation, and explicitly distinguish PAK from Local handling. Add-on/mod documentation independently installs replacements into `Data/Local`.

The clean core therefore implements:

```text
Data/Local/<logical resource path>   (highest priority)
                ↓ fallback
Data/Paks/*.pak/<logical resource path>
```

Local-over-PAK precedence is evidence-backed. If multiple PAKs collide, the current clean implementation uses later-added PAK precedence as a deterministic policy; this is not yet claimed to match historical ordering.

## PAK container

All four recovered 1.0.6 PAKs are ordinary ZIPs and every canonical member is method 0/stored. The clean reader intentionally supports only this proven ZIP subset and validates CRC32. Real-corpus validation passes all **871 original files**.

## Resource IDs

Resource filenames conventionally end with `[xxxx]` before the extension. Exactly four bytes are significant. Preserve case **and whitespace**.

Examples:

- `Level 01[le01].leve`
- `Player 1[pl01].plde`
- `Bop[bop ].IMA`
- layer ID `air ` (trailing space is semantic)

Do not normalize identifiers unless executable behavior proves normalization.

## Image plates

`im08` resources commonly occur in pairs:

- `... IA[TAG].gif`
- `... IC[tag].gif`

The original application diagnostic `Sprite color and alpha plates are not equal size` independently corroborates the mod/resource convention. Current mapping:

- IA = alpha plate
- IC = color plate

Tag case remains literal.

## Known format buckets

- `im08` — GIF alpha/color sprite plates
- `im16` — 16-bit TGA images
- `soun` / `.IMA` — AIFC/IMA4 audio
- `leve` — level definitions
- `unde` — unit/entity definitions
- `wede` — weapon definitions
- `plde` — player definitions
- `film` — replay/input recordings
- `idli` — ID lists
- `flli` — float/global constant list
- `coli` — color list
- `tefo` — text format
- `stli` — string list
- `reli` — rect list
