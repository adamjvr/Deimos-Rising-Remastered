# Audio / Music Resource Runtime

## Scope

This milestone reconstructs the portable decode boundary for the canonical Mac
1.0.6 compressed-audio resources. It does **not** yet reconstruct Sound Manager
channel scheduling, volume/pitch arbitration, double-buffer timing, or modern
platform playback.

The canonical executable imports classic Sound Manager entry points including
`SndNewChannel`, `SndPlayDoubleBuffer`, `SndDoImmediate`, `SndDisposeChannel`,
`SndGetInfo`, and `GetCompressionInfo`. The clean core instead decodes the
content to interleaved signed 16-bit PCM so later native audio backends can be
platform-independent.

## AIFC container

All canonical `Audio.pak` and `Music.pak` compressed resources are `FORM/AIFC`
with compression FourCC `ima4` and 16-bit decoded PCM.

The clean parser reads:

- `COMM` channel count;
- the raw QuickTime IMA4 packet-group count stored in the COMM frame field;
- sample size;
- 80-bit extended sample rate;
- compression FourCC;
- `SSND` offset and payload.

### Legacy FORM-size tolerance

Canonical `Music 3[mu03].aif` is internally inconsistent in a way accepted by
QuickTime: its FORM length is **76 bytes shorter** than the actual valid chunk
stream. The `SSND` chunk itself is complete and reaches the real end of the
resource.

The clean parser therefore rejects FORM sizes that exceed the input, but treats
an under-declared FORM length as advisory and continues over complete chunks in
the supplied resource payload. This behavior is regression-bound so the stock
`mu03` master remains readable.

## Apple / QuickTime IMA4 packet format

One channel packet is 34 bytes and yields 64 PCM samples:

```text
bytes 0..1   big-endian state header
bytes 2..33  32 packed ADPCM bytes
```

Header bits:

```text
15........7  top 9 bits of signed 16-bit predictor
6.........0  step-table index
```

Nibbles decode **low first, then high**.

A subtle state rule is required for bit/sample-exact decoding. Because the
packet header carries only the top nine predictor bits, the decoder keeps the
more precise running predictor when:

- the packet step index equals the current step index; and
- the packet predictor differs from the running predictor by no more than
  `0x7f`.

Otherwise the header resets both predictor and index. With this rule the clean
decoder matches an independent FFmpeg decode of all three canonical Music.pak
resources sample-for-sample.

For canonical Deimos AIFC/IMA4 files, the COMM frame field is the number of
64-sample packet groups. Therefore:

```text
decoded frames = COMM count * 64
```

Stereo stores one 34-byte packet per channel for each packet group; output is
interleaved after decoding both channel packets.

## Canonical Music.pak

| Resource | Channels | Rate | Packet groups | PCM frames | PCM CRC32 (little-endian s16) |
|---|---:|---:|---:|---:|---:|
| `Music 3[mu03].aif` | 2 | 44100 | 134892 | 8633088 | `4f945e4e` |
| `Ambient Music Loop[ammu].IMA` | 2 | 44100 | 23966 | 1533824 | `9871dd60` |
| `Interface Music Loop[inmu].IMA` | 2 | 44100 | 41153 | 2633792 | `60d31157` |

All three decode through the clean core and the reference probe checks these
metadata/count/checksum contracts when sibling `Music.pak` is present next to
`Game.pak`.

## Canonical Audio.pak

The complete stock effects corpus is homogeneous:

- 96 resources;
- all AIFC/`ima4`;
- all 16-bit decoded PCM;
- all 44.1 kHz;
- all mono;
- 48,959 packet groups total;
- 3,133,376 decoded PCM frames total.

Every resource decodes successfully through the same clean decoder. The
reference probe checks the corpus count and total decoded frame count whenever
sibling `Audio.pak` is available.

## DR-EVID-005 soundtrack oracle

The standalone soundtrack archive gives a useful second source for semantic and
content identity. In particular:

- Theme Song ID3 title = `Music 3[mu03]`;
- Deimos Game 3 is the same underlying program material as `mu03`;
- Deimos Interface contains the `inmu` loop;
- Deimos Advertising contains the `ammu` loop.

The MP3 files remain outside Git and milestone ZIPs. Only hashes, metadata, and
correlation observations are retained.

## Remaining audio work

Still open:

- map the executable's Sound Manager channel/priority ownership;
- reconstruct effect min/max volume and pitch application at playback;
- identify exact music-loop start/restart orchestration;
- bind player/entity/destruction audio events into the full headless world;
- implement a native platform mixer/output backend;
- characterize whether the APPL/INST chunks on `ammu`/`inmu` contribute loop
  points or whether the engine handles looping externally.
