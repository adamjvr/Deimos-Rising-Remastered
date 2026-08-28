# Deimos Rising Remastered

Evidence-driven, clean-code reconstruction and native remaster of **Deimos Rising**.

## Current reconstruction baseline

The evidence corpus now includes:

- an older StuffIt/disc-image distribution (`DR-EVID-001`);
- a fully extracted Mac **1.0.6** installation (`DR-EVID-002`);
- a Windows PE32/NSIS distribution (`DR-EVID-003`);
- an add-ons/update/reference/mod/music corpus (`DR-EVID-004`).

The Mac 1.0.6 install gives us the complete canonical game resource set: all four PAKs, twelve levels, unit/weapon/player definitions, sprites, interface art, sound, music, replay films, and the PowerPC PEF application/resource fork.

## Reconstruction rule

**Game code is independently reconstructed. Original supplied art/audio/data are intentionally used as the canonical asset baseline until restored/upscaled replacements are produced.** See `docs/CLEAN_ROOM.md` and `docs/ASSET_POLICY.md`.

## Clean implementation

The first portable core code lives under `include/` and `src/`. It begins with the recovered four-character resource-ID/plate namespace so later loaders and gameplay systems can reference original and restored assets through the same stable identity.

Build the current clean core/tests with:

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

See `docs/STATUS.md` and `docs/ROADMAP.md` for the active reverse-engineering fronts.
