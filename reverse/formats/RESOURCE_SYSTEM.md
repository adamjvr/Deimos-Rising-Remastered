# Resource system — working specification

Status: **strong**, with several points confirmed by application diagnostics and on-disk structure.

## Providers

The original install contains:

- packaged resources under `Data/Paks/*.pak`;
- loose override/category directories under `Data/Local/*`.

Application diagnostics state that ZIP files are supported in Paks and not Local. The recovered `.pak` files are normal ZIPs. Reconstruction should therefore model resource lookup as providers with Local capable of overriding packaged resources.

## Resource IDs

Resource filenames conventionally end with `[xxxx]` before the extension. Exactly four bytes are significant. Preserve case and spaces.

Examples:

- `Level 01[le01].leve`
- `Player 1[pl01].plde`
- `Bop[bop ].IMA`

Do not normalize to lowercase until executable behavior proves lookup is case-insensitive.

## Image plates

`im08` resources commonly occur in pairs:

- `... IA[TAG].gif`
- `... IC[tag].gif`

Application diagnostics refer to sprite `color and alpha plates`; current mapping is therefore:

- IA = alpha plate
- IC = color plate

The exact relationship between tag case and plate lookup remains to be proven. Preserve both literally.

## Known format buckets

- `im08` — GIF sprite/image plate resources
- `im16` — TGA image resources
- `soun` / `.IMA` — AIFC/IMA4 audio
- `leve` — level definition
- `unde` — unit/entity definition
- `wede` — weapon definition
- `plde` — player definition
- `film` — deterministic/demo recording candidate
- `idli` — ID list/table
- `flli` — game/global list candidate
- `coli` — color table
- `tefo`, `stli`, `reli` — semantics under active reconstruction

The custom data formats are not to be treated as understood merely because filenames are known.
