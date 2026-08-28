# Destruction and group runtime — Mac 1.0.6

The clean core now contains a bounded reconstruction of PPC `0x16300`,
`0x36120`, `0x363C0`, `0x364F0`, and the outer cleanup pass around `0x36610`.
The detailed binary notes live in
`reverse/formats/DESTRUCTION_GROUP_RUNTIME.md`.

Implemented behavior includes destruction spawn/particles/notice/sound facts,
terrain-draw and obstacle requests, ordinary/group-kill coin rewards, random
bonuses, owner/child cascades, pickup reward suppression, original/active/
destroyed group counters, and the special `SERM` group-removal exemption.

Random bonus resources are bound directly from canonical positional tables:
`Game[gafl]` 209–219 and `Objects[gaob]` 25–34. The clean binder verifies the
resource labels and reproduces the 1.0.6 thresholds
`70,78,82,84,87,91,95,98,100`, ground-accuracy threshold `10`, and minimum
progression `3` for the `rb09/rb10` tail.

Lethal ordinary collision and successful pickup paths can now run `0x16300`
immediately when supplied a removal context, preserving random-bonus RNG order.
The later group-removal pass is idempotent with respect to already-processed
destruction effects.

Canonical `Game.pak` currently contains 99 destruction-spawn units, 99
particle units, 77 destruction sounds, 28 ordinary coin-reward units, 15
group-kill reward units, 54 destroy-children units, 58 delete-children units,
13 obstacle creators, 32 terrain-draw units, and 7 random-bonus units.

Ground-sensitive helper `0x16880` and the persistent ground-obstacle Rect store are now reconstructed. Remaining boundaries are complete ground-obstacle rollback/render integration, renderer/terrain mutation beyond that Rect store, the special live `+0xCD` path, concrete player pickup/damage mutation, and full game-tick orchestration of every destruction entry site. See `TERRAIN_MEDIA_RUNTIME.md`.
