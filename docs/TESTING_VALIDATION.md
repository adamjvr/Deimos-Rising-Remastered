# Testing and Validation

## Evidence integrity

Every supplied archive receives stable MD5/SHA-1/SHA-256 identity before transformation. Extracted containers, executables, resource forks, PAKs, and important derived payloads receive independent hashes. Version/build relationships are established by internal evidence where possible rather than filenames alone.

## Resource-system tests

Clean code must preserve four-character tags byte-for-byte, including case and spaces. IA/IC plate parsing is tested independently of image decoding. Local-vs-PAK precedence will receive explicit tests before it is relied upon by gameplay code.

## Serialization tests

Recovered game-data formats are not accepted from guessed C structs. Each parser must have:

- bounds-checked reads;
- field-by-field evidence notes;
- synthetic fixtures;
- tests for malformed/truncated data;
- round-trip tests where serialization is reconstructed;
- cross-file reference validation against known four-character tags.

## Replay/behavior validation

The corpus contains four canonical built-in `.film` resources and ten additional declared demo films. Once the film format is decoded, these recordings become high-value deterministic regression inputs for:

- player motion and firing;
- enemy/unit sequencing;
- collision/damage;
- level timing/scrolling;
- weapons/projectiles;
- scoring and pickups;
- RNG/timing behavior where encoded or inferable.

Recorded reference outcomes are compared to the clean simulation. Discrepancies are logged rather than normalized away.

## Platform parity

Portable-core tests run identically on macOS, iPadOS host-compatible test targets where practical, Linux, and Windows. Rendering/input/audio adapters may differ, but gameplay/resource semantics must not.
