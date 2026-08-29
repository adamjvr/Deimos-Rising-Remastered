# DR-EVID-005 — standalone soundtrack archive

## Source

User-supplied `DeimosRising_soundtrack.sit`.

- size: 31,157,858 bytes
- MD5: `9c80e6bb893262b50e6b77aa004529bc`
- SHA-1: `a62d40dcbd5cffa5c25c5a882e5468f7adfc63a7`
- SHA-256: `e1f553997de3093a99b01ece0797704056bb636ae51fb258c672379e7cf3b347`
- container: StuffIt 5
- entries: 10
- all ten MP3 data forks use StuffIt method 0 (stored)
- all entry creation/modification timestamps: 2004-08-12 18:00:00 UTC

The audio payloads themselves are evidence and are not committed to the clean repository.
Exact per-track hashes and metadata are recorded in
`evidence/manifests/DR-EVID-005.json`.

## Contents

The archive contains:

- Deimos Advertising
- Deimos Ambient
- Deimos Game 1
- Deimos Game 2
- Deimos Game 3
- Deimos Game 4
- Deimos Game 5
- Deimos Game 6
- Deimos Interface
- Deimos Theme Song

All are 44.1 kHz stereo MP3 files. Nine filenames were already present in the
DR-EVID-004 add-ons corpus catalog. DR-EVID-005 additionally exposes the Theme
Song directly as an MP3 rather than only through nested theme-song archives.

## Canonical Music.pak correlation

The strongest identity result is the theme/game-3 material:

- `Deimos Theme Song.mp3` carries ID3 title **`Music 3[mu03]`** and encoder
  **`iTunes v1.0`**.
- `Deimos Game 3.mp3` is essentially a lossy encode of the same program
  material as canonical `Music 3[mu03].aif`; decoded full-overlap waveform
  correlation is approximately 0.999983.
- the Theme Song is a separate MP3 encode of the same source; its best decoded
  alignment is roughly +24 ms and its long-term envelope is essentially the
  same.
- `Deimos Interface.mp3` contains the canonical
  `Interface Music Loop[inmu].IMA` material as a strongly correlated segment.
- `Deimos Advertising.mp3` contains the canonical
  `Ambient Music Loop[ammu].IMA` material as a strongly correlated segment.
- despite its filename, `Deimos Ambient.mp3` is not a direct encode of `ammu`.

Every canonical numbered level currently observed references music ID `mu03`,
so the soundtrack archive independently identifies the actual level-music
master rather than merely supplying a similarly named song.

## Reverse-engineering value

DR-EVID-005 is useful as a second-source audio oracle:

1. it gives human-readable names/metadata for material otherwise identified by
   FourCC;
2. it independently binds `mu03` to the released theme/game-3 master;
3. it provides lossy encodes against which decoded canonical AIFC/IMA4 audio can
   be correlation-tested;
4. it separates distributed soundtrack arrangements from the shorter loop
   resources actually stored in `Music.pak`.
