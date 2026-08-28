# Provenance

Evidence is assigned stable IDs and never merged destructively merely because filenames look similar.

## DR-EVID-001

User-supplied `deimos-rising-install.sit`.

- MD5: `c3b36b3ef326486176e6d5d90af91032`
- SHA-1: `71919aa86a83334e25f060c5ae86bcbb7a3b0e3f`
- SHA-256: `3ed7a640aded708fa3720c18d11d1a3f6ff23348888a07043401832046563100`

Contains `deimos_rising.iso` (104,851,456 bytes uncompressed). External archival metadata associates this exact SHA-1 with v1.0.3; that label remains provisional until internally confirmed.

## DR-EVID-002

User-supplied `DeimosRising.sit`.

- MD5: `cc6607355eb6584adf7cf9115a11292f`
- SHA-1: `c434bc0972a064dc8a6dbb4583fad5839f2fa0e5`
- SHA-256: `dc75c002995d8bfdd3124bda433efded2698343b1f1a2fa171b9a33c0a4b2f77`

Contains `Deimos Rising 1.0.6.smi`. The extracted HFS volume is internally named `Deimos Rising 1.0.6` and contains the complete game installation. The relocated PowerPC startup path independently references the embedded build strings `1.0.6`, `Jan  2 2004`, and `11:55:01`, internally confirming both the version and build timestamp carried by this evidence set.

## DR-EVID-003

User-supplied `win_deimosrising.zip`.

- MD5: `dc78526079a72076a647ff33cfcad73c`
- SHA-1: `f223a797c5d8dcdb60466df69a878ac3780db8c0`
- SHA-256: `82298d68c2cd6c530ad857cb4002a4f3f487a5b95ce39b94f4c2b16d5bcf195e`

Contains a 35,270,167-byte PE32/i386 NSIS installer. The PE timestamp is 2003-02-10 17:58:53 UTC. This build is retained as a cross-platform behavioral and structural oracle.

## DR-EVID-004

User-supplied `deimos_addons.sit`.

- MD5: `2951e11ea8ede2cd3959467f46078917`
- SHA-1: `62eaf4dc8947ba738bab07651862972a4168e957`
- SHA-256: `2f24df9bb5ef67d0c3a0b97f7ad8639484f37276555b58562b544a2fa648651d`

Contains official update/reference material, demo recordings, art, music, desktops, and community modifications. The Apple Bundle 1.0.6 updater has been decompressed and its HFS volume extracted separately from the base 1.0.6 game.

See the machine-readable manifests under `evidence/manifests/` for exact extracted hashes and structural details.
