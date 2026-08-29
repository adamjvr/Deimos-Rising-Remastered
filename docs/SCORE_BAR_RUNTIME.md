# Score-bar runtime — Deimos Rising 1.0.6

## Scope

This note records the clean-room reconstruction of the normal gameplay score-bar
producer/cache path that feeds source-canvas X=416..575 before the mode-1
presentation routine `0xBEB0` copies that 160x480 region to the display.

Primary PPC cluster:

- `0x30F40` score-bar object initialization;
- `0x31400` level/session setup and initial cache population;
- `0x31710` direct cached shield setter;
- `0x31760` direct cached power setter/clamp;
- `0x317E0` per-tick cache/dirty updater;
- `0x31AE0` dirty-region draw dispatcher;
- `0x31D70` score text;
- `0x31EA0` life symbol;
- `0x32050` life count;
- `0x32250` shield meter;
- `0x32500` power meter;
- `0x327B0` weapon preview;
- `0x32A70` dirty-region helper.

This document concentrates on semantic state and dirty-region production. The original-pixel execution of those dirty regions is now closed separately in `SCORE_BAR_PIXEL_RUNTIME.md`, using the canonical `scor` TGA and `Interface.pak` `tesm` glyph atlas.

## Static Game[gafl] contract

Zero-based indices 111..143 are a contiguous score-bar block and are
label-validated by `compile_legacy_score_bar_config()`:

- score spacing: 13;
- lives symbol positions: P1 `(534,41)`, P2 `(534,276)`;
- shield positions: P1 `(495,124)`, P2 `(495,359)`;
- shield increase/decrease rates: `2 / 3`;
- power positions: P1 `(495,159)`, P2 `(495,394)`;
- power increase/decrease rates: `2 / 4`;
- weapon positions:
  - P1 `(467,199)`, `(502,199)`, `(530,199)`;
  - P2 `(467,434)`, `(502,434)`, `(530,434)`;
- non-selected/selected weapon blend amounts: `16 / 6`;
- non-selected weapon scale: `0.7`;
- maximum displayed life count: `9`.

These X positions are in the shared source canvas. Subtracting the gameplay
width 416 yields positions inside the 160-pixel score-bar panel.

## Rects[inre] panel-local contract

The first sixteen Rect-list entries are eight dirty/background regions per
player:

| index | region | rect |
|---:|---|---|
| 0 | P1 score | `<25,81,135,95>` |
| 1 | P1 life symbol | `<98,22,138,62>` |
| 2 | P1 life count | `<56,19,102,63>` |
| 3 | P1 weapon 1 | `<33,181,65,216>` |
| 4 | P1 weapon 2 | `<76,188,95,210>` |
| 5 | P1 weapon 3 | `<103,188,124,210>` |
| 6 | P1 shields | `<31,117,127,132>` |
| 7 | P1 power | `<31,152,127,167>` |
| 8 | P2 score | `<25,317,135,333>` |
| 9 | P2 life symbol | `<98,257,138,297>` |
| 10 | P2 life count | `<56,263,102,307>` |
| 11 | P2 weapon 1 | `<33,416,65,451>` |
| 12 | P2 weapon 2 | `<76,423,95,445>` |
| 13 | P2 weapon 3 | `<103,423,124,445>` |
| 14 | P2 shields | `<31,353,127,368>` |
| 15 | P2 power | `<31,388,127,403>` |

The original block owns eight rectangles but only six dirty classes because
all three weapon rectangles share one dirty byte.

## Player Definition score-bar resources

Compiled Player Definition `+0x30..+0x44` is six words copied into the score-bar
player block during `0x31400`:

- `spriteScoreBar_ID` / `spriteScoreBarFrame_INT`;
- `spriteScoreBarPower_ID` / `spriteScoreBarPowerFrame_INT`;
- `spriteScoreBarShield_ID` / `spriteScoreBarShieldFrame_INT`.

Canonical players use base `play` frame 0 and `shme` power/shield frames 1/0.
The clean `CompiledPlayerRuntimeDefinition` now preserves these fields.

Weapon Definition fields `scoreBarPreviewFace_ID` and
`scoreBarPreviewFrame_INT` are the preview descriptor copied by weapon-handler
helper `0x3BB40`. All five canonical weapons expose a score-bar preview.

## Six dirty classes

The per-player block is 332 bytes (`0x14C`). The live dirty bytes are:

- `+0x128`: score;
- `+0x129`: life symbol;
- `+0x12A`: life count;
- `+0x12B`: all three weapon previews;
- `+0x12C`: shield;
- `+0x12D`: power.

`0x31400` marks all six dirty on initial setup. `0x317E0` clears all six at the
start of each tick, then sets only classes whose cached semantic values require
work.

A lives-number change dirties the life-count region only; it does not by itself
dirty the life-symbol region.

## Visibility latches

The block also carries:

- `+0x12E`: previous/present player latch;
- `+0x12F`: content-visible/fade selector.

Normal updater work requires player enabled and status != 1 (`game_over`). On
the first transition from present to absent/game-over, `0x317E0` clears
`+0x12F`, dirties all six classes, and clears `+0x12E`. Subsequent absent ticks
do not repeatedly dirty the bar.

The original draw routines have element-specific fade behavior, so clean code
keeps the visibility facts separate from the semantic cache instead of
pretending all elements share one generic alpha rule.

## Score and life cache

`0x299F0` is the semantic score getter from live player `+0xB0`.
`0x26D50` is the semantic lives getter from live player `+0x98`.

The clean `PlayerRuntimeSlot` therefore now owns `score` in addition to `lives`.

`0x32050` displays:

`clamp(lives - 1, 0, ScoreBar_Lives_MaxNumDisplayed)`

With the canonical maximum semantic lives of 10 and score-bar display cap 9,
this exactly explains why a fully stocked player displays `9` extra-life
symbols/count rather than `10`.

## Shield smoothing

Target shield comes from `0x27540`, the already recovered semantic shield
percentage.

When cached shield differs:

- dirty shield;
- if cached > target: subtract 3 every tick and clamp at target;
- if cached < target and status == 4 (`active`): add 2 and clamp at target;
- waiting/dying players therefore permit downward convergence but suppress
  upward refill animation.

## Power smoothing and exact bounds

Weapon-handler `0x3BB20` returns the displayed power target from handler
`+0x24`.

When cached power differs:

- dirty power;
- if cached > target: subtract 4 and clamp at target;
- if cached < target and status == 4: add 2 and clamp at target.

After convergence, PPC `0x31A38..0x31A64` clamps the cached value. Relocated PEF
TOC data resolves the constants exactly:

- lower: `0.0f`;
- upper: `100.0f`.

This removes the previous inference and makes the clean clamp evidence-backed.

## Weapon preview dirty gate

Weapon-handler `0x3BB30` exposes a one-byte preview-changed flag. Only when it
is set does the score-bar updater dirty the weapon region and refresh the three
face/frame pairs with `0x3BB40`.

`0x327B0` uses the first preview as the selected/current presentation:

- slot 0: selected blend amount 6, normal scale;
- slots 1/2: non-selected blend amount 16 and scale 0.7.

The clean runtime preserves the preview descriptors and the explicit changed
signal rather than comparing them every tick, matching the binary producer
contract.

## Score production and extra-life thresholds

The score-bar cache consumes `0x299F0`, but `0x29A10` defines how that score is
produced. The clean player runtime now implements this producer.

Player Definition compiled fields:

- `+0x68` `life_InitialRequiredScore_INT` = canonical 10000;
- `+0x6C` `life_AdditionalRequiredScore_INT` = canonical 30000.

Game[gafl] index 182:

- `Player_ExtraLifeScoreAdjustment` = canonical 10000.

Normal `r5 == 0` awards:

1. multiply requested points by the live bonus multiplier;
2. add to score;
3. only for positive requested points, test `newScore > nextThreshold`;
4. equality does **not** award a life;
5. at most one threshold is consumed per call, even if one award jumps multiple
   nominal thresholds;
6. increment lives only if below PlayerDef maximum and request `life_Spawn_ID`;
7. advance threshold by `life_AdditionalRequiredScore + currentAdjustment`;
8. increment adjustment by `Player_ExtraLifeScoreAdjustment`.

The binary's `r5 != 0` branch is retained under the deliberately narrow clean
name `raw_score_mode`: it bypasses the multiplier and, for positive requested
points, sets the adjustment field to `newScore + Game[182]`. The higher-level
mode meaning remains intentionally unresolved until its special caller is
fully named.

## Clean implementation

Files:

- `include/deimos/score_bar_runtime.hpp`;
- `src/core/score_bar_runtime.cpp`;
- `tests/score_bar_runtime_test.cpp`;
- score producer additions in `player_runtime.*` / `PlayerRuntimeSlot`;
- pixel consumer and original-asset oracle documented in `SCORE_BAR_PIXEL_RUNTIME.md`.

The score-bar compiler rejects shifted/modded tables whose labels no longer
match the proven 1.0.6 positional contract. Regression coverage freezes the
six dirty classes, exact geometry, resource descriptors, 2/3 and 2/4 meter
rates, 0..100 power clamp, hide transition, displayed-lives formula, score
multiplier/threshold behavior, strict threshold comparison, one-threshold-per-
call quirk, and raw-score branch.
