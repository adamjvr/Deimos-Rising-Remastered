# Original Asset Policy

The reconstruction uses the **original Deimos Rising art, audio, interface, and data assets supplied as evidence as the canonical visual/audio baseline** while executable/gameplay logic is independently reconstructed.

This is distinct from the clean-room code rule. Clean-room means that new implementation code is written from observed formats and behavior rather than copied executable code. It does **not** mean discarding the original assets.

## Development tiers

1. **Canonical original** — recovered original 1.0.6 resources and PAKs. These are the fidelity reference and initial runtime assets.
2. **Restored/upscaled** — derived replacements that retain the same semantic four-character resource ID and behavior.
3. **Replacement/free asset pack** — optional future distribution path where needed.
4. **Community add-ons/mods** — preserved separately; never silently promoted to canonical game content.

The engine should make asset substitution explicit. A restored asset must be able to fall back to the original resource while reconstruction is incomplete.

## Resource identity

Observed assets use human-readable filenames plus a four-character resource tag in brackets. The tag is data, not decoration. Preserve its exact four bytes, including case and spaces.

Examples:

- `Player 1 Blue IA[PL1B].gif` — alpha plate
- `Player 1 Blue IC[pl1b].gif` — color plate
- `Level 01[le01].leve`
- `Bop[bop ].IMA` — trailing space is significant

Application diagnostics explicitly describe sprite **color and alpha plates**, which corroborates the IA/IC pairing.
