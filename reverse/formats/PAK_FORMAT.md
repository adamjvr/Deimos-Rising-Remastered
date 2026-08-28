# PAK container format

Status: **confirmed for the canonical Mac 1.0.6 installation**.

`Audio.pak`, `Game.pak`, `Interface.pak`, and `Music.pak` are conventional ZIP archives. Every canonical member uses ZIP compression method **0 (stored)**; no Deflate implementation is required to load the original 1.0.6 asset set.

Clean-core support therefore implements only the evidence-backed subset:

- single-disk ZIP;
- central directory + local headers;
- unencrypted members;
- method 0/stored members;
- exact path lookup;
- CRC32 validation.

It fails closed on compressed/encrypted/unsupported ZIP variants rather than silently broadening the reconstructed contract.

Real-corpus validation through the clean reader:

| PAK | Actual files CRC-validated |
| --- | ---: |
| `Audio.pak` | 96 |
| `Game.pak` | 763 |
| `Interface.pak` | 9 |
| `Music.pak` | 3 |
| **Total** | **871** |

`Game.pak` also contains directory entries, hence 776 total central-directory entries versus 763 actual files.

## Provider behavior

Original executable diagnostics and mod documentation establish `Data/Paks` and `Data/Local` as distinct providers. Loose Local resources override packaged resources. The clean `ResourceStore` implements that precedence. Cross-PAK collision precedence is explicitly treated as an implementation policy until executable evidence proves the historical ordering.
