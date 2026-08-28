# Render / visual-state runtime correspondence — Mac 1.0.6

Status: **PPC and parser-correlated through the renderer request boundary**.

## Function map

| PPC offset | Recovered role |
| ---: | --- |
| `0x12650` | initialize shared 0x94-byte sprite base |
| `0x12750` | visibility + tint scalar ramps |
| `0x12840` | scale scalar ramp; marks geometry dirty |
| `0x12940` | refresh scaled dimensions / half extents |
| `0x12F20` | draw wrapper; shadow pass before main pass |
| `0x12FA0` | main sprite/tint/glow request builder |
| `0x13460` | shadow request builder |
| `0x146F0` | entity state entry / visual reset path |
| `0x19AD0` | sprite/frame cached-handle lookup boundary |
| `0x1A260` | integer percent -> legacy scale factor |
| `0x33E0C` | current-state visual target refresh |
| `0x34AC8..0x34AE4` | temporary shadow-only selector choreography |
| `0x34B48..0x34B5C` | temporary main-only selector choreography |

## Shared sprite-base offsets

`0x12650` initializes a shared base embedded at object offset zero. Relevant
members: face/frame `+0x1C/+0x20`, dimensions `+0x24/+0x28`, half extents
`+0x2C/+0x30`, dirty `+0x34`, terrain flag `+0x36`, pass selectors
`+0x37/+0x38`, layer `+0x4C`, cached sprite `+0x50`, colorise `+0x54`, tint
triplet `+0x58/+0x5C/+0x60`, tint color `+0x64`, visibility triplet
`+0x68/+0x6C/+0x70`, glow `+0x74/+0x78/+0x80`, scale triplet
`+0x84/+0x88/+0x8C`, terrain submission sequence/cache `+0x90`.

Live `+0x18` starts at 1 and the entity constructor clears it for `drawLayer_ID
== 'hud '`. Live `+0x19` and `+0x1A` are entity-specific overlays already
proven as the air-domain cache and `adjustShadowLocForScaling_BOOL`.

## Parser anchors

UnitDef:

- `+0x11E` casts shadows;
- `+0x12C` adjust shadow location for scaling;
- `+0x1AC` initial scale percent;
- `+0x1B0` initial scale tolerance percent;
- `+0x1B4` initial visibility percent;
- `+0x2E0` draw layer.

State base = `UnitDef +0x4E0 + stateIndex*0x5E0`:

- `+0x304` sprite face;
- `+0x30C/+0x310` frame min/max;
- `+0x324` use parent direction;
- `+0x332` tint color;
- `+0x34D` do colorise;
- `+0x353` draw to terrain;
- `+0x3BC/+0x3C0` required scale / scale delta;
- `+0x3C4/+0x3C8` required visibility / visibility delta;
- `+0x3CC/+0x3D0` tint / tint delta.

## Layer switch correction

PPC literal construction at `0x1318C..0x131D8` uses one `lis 0x706c` base and
forms `plsh`, `plef`, `play`, `plwe`, and `plui`. Earlier working notes that
transcribed the middle two as `moay/mowe` were incorrect. The main-layer switch
is therefore:

`defa: 3 ground / 7 air; grou:3; grhi:5; ailo:7; aihi:8; plwe:9; play:10;
plsh:11; plef:12; plui:13; atmo:14; hud:15`.

Zero/`none` is rewritten to `defa` at `0x130C4..0x130E4` before this switch.

Shadow builder `0x13460` has its own numeric layer assignment: default ground
and `grou` use 2, `grhi` uses 4, while default air and recognized air/player/HUD
layers use 6. Do not reuse main-sprite layer values for shadows.

## Submission facts

`0x12F20` checks visibility first, then shadow selector/global-shadow gate, then
main selector. `0x12FA0` submits base only when live `+0x54 == 0`, then a tint
pass when `+0x58 > 0`, then a collision-glow pass when `+0x74 != 0`.

State-draw-to-terrain (`+0x36`) bypasses normal main-layer assignment and uses
`0x5CE0`, live `+0x90`, flag bit 8, and a terrain/backend preparation call.
The precise raster target remains unresolved and is not represented as a fake
pixel implementation in the clean core.

## Clean boundary

`render_runtime.{hpp,cpp}` compiles these source fields, reproduces the scalar
ramps and scale-tolerance RNG, maps main/shadow layers, and emits ordered
headless render intents. No platform graphics API is present in this layer.
