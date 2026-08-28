# Entity core-edge live flags — Mac 1.0.6

Status: **binary-confirmed**.

## Live member `+0x19`

Constructor `0x35F88..0x35FA0` compares derived `UnitDef +0x08` to FourCC
`air ` and writes the Boolean result to live member `+0x19`:

```text
+0x19 = (UnitDef+0x08 == 'air ')
```

`UnitDef +0x08` is the already-proven `air `/`grnd` domain derived from
`isGroundBased_BOOL`. Main tick `0x344F8` rejects `+0x19 != 0` before testing
UnitDef `+0x128` (`collidesWithGroundObstacles_BOOL`) and calling `0x2A830`.
Therefore the field is a cached air-domain/layer flag, not a rollback-state or
terrain-mode byte.

## State `+0x356` and live member `+0xCD`

State parser call:

```text
0x41698  parser context
0x416A0  key pointer = relocated string offset +0x20BD
0x416A4  destination = state +0x356
0x416A8  Boolean parser 0x2CA40
```

The relocated key is exactly
`#stateUseThisStateOnShieldDepletion_BOOL`.

Member constructor `0x35DAC..0x35DF0` clears live `+0xCD`, scans every state at
UnitDef-relative `+0x836 + i*0x5E0` (`state +0x356`), and sets `+0xCD = 1` when
any marked state is found.

Damage routine `0x14F10`:

```text
0x1504C..0x15058  score award
0x15060           read live +0xCD
0x1506C..0x15078  +0xCD == 0 -> ordinary destruction 0x16300
0x15084..0x1508C  +0xCD != 0 -> 0x17E70(entity,currentTick)
```

`0x17E70` scans `UnitDef +0x836 + i*0x5E0` in file order and calls `0x146F0`
on the first marked state (`UnitDef +0x97C + i*0x5E0` is its state record
passed to the state-entry routine). No ordinary destruction call follows that
branch.

Canonical stock `Game.pak`: 0 marked states / 0 affected units. The path is
retained for executable compatibility and custom/legacy content.
