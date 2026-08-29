# Level-Select Acceptance / Failure Effect Runtime

Status: **binary-confirmed front-end visual subsystem** for Deimos Rising 1.0.6.

This runtime is intentionally separate from `GAMEPLAY_FRAME_RUNTIME.md`. The
remaining `COST` overlay near the gameplay score-bar code is owned by the level
selection screen; it is not an unmodeled in-game HUD layer.

## Binary anchors

- `0x2FC90` — reset/default effect state
- `0x2FCC0` — per-tick blend/scale update
- `0x2FE40` — trigger/reset entry point
- `0x2FB88..0x2FC14` — immediate `COST` rectangle submission
- `Formats[gate]` ordinals 27/28 — `lsca` / `lscf`
- `Game[gafl]` 44..47 — acceptance/failure scale rates and maxima

`Formats[gate].idli` proves the runtime ordinal mapping:

- 27: `LevSel_ColorStrip_Acceptance <lsca>`
- 28: `LevSel_ColorStrip_Failure <lscf>`

## Trigger contract

`0x2FE40` accepts these modes:

| mode | effect |
| ---: | --- |
| 0 | reset / inactive |
| 1 | acceptance |
| 2 | failure |
| other | leave current state unchanged |

Acceptance loads the `lsca` text-format color/blend; failure loads `lscf`.
Canonical 1.0.6 values are:

- acceptance: green xRGB1555 `0x03e0`, blend `16`
- failure: red xRGB1555 `0x7c00`, blend `16`

Both begin with scale `0.0` and the direction latch set to expanding.

## Update contract

The live state is the compact structure used by `0x2FC90..0x2FE18`:

- active kind: inactive / acceptance / failure
- xRGB1555 color
- blend amount in the legacy `0..32` destination-weight domain
- scale phase
- expanding/shrinking latch

Every active tick:

1. blend increments by one toward `32` and clamps there;
2. acceptance selects `Game[gafl]` 44/45 (`0.18`, `2.0`);
3. failure selects 46/47 (`0.25`, `2.0`);
4. scale expands from zero to the selected maximum;
5. reaching the maximum flips the direction latch;
6. scale then shrinks back to zero;
7. teardown occurs only when `scale == 0` **and** `blend == 32`.

Canonical pulse lengths from trigger through teardown are therefore 24 ticks
for acceptance and 16 ticks for failure.

Teardown restores the exact reset state: inactive, white `0x7fff`, blend 32,
scale zero, direction latch clear.

## Draw contract

The draw path scales the target rectangle about its integer center, using
truncation matching the PPC `fctiwz` conversion, then submits the shared
renderer special face `COST`. The request carries the effect color in the
special-color field and the live blend amount in the normal effect field.
A zero scale or blend 32 produces no visible request.

The clean runtime exposes the effect as a portable rectangle/request plan. It
does not preserve QuickDraw ownership or front-end widget objects; those are
platform integration details outside the deterministic effect semantics.
