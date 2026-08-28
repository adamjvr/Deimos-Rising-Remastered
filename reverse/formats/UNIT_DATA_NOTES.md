# Unit / weapon / player data notes

Status: **typed structural reconstruction complete; behavioral interpreter in progress.**

The original `.unde`, `.wede`, and `.plde` resources use the recovered legacy
tagged-text serialization.  The clean core now parses all 386 canonical units,
all 5 weapons, and both player definitions directly from `Game.pak`, including
repeated states, spawn sets, rules, and weapon spawns.

For current counts, condition vocabulary, action-resolution rules, cross-resource
integrity, and intentionally unresolved behavior, see
`UNIT_BEHAVIOR_MODEL.md`.
