# WIP9 — Flee Targets, Target/Motion Closure, and Enemy-Fire Heading

Status: **implementation complete; freeze validation pending at time of writing**.

WIP9 closes the remaining shipped 1.0.6 target/flee edge semantics and the missing
visual-heading input to rotation-adjusted entity spawns. The work is evidence-driven:
the original PowerPC PEF was recovered again from the canonical StuffIt distribution
and the relevant routines were re-disassembled before changing the clean runtime.

## Original executable recovery witness

The local evidence chain is:

1. canonical `DeimosRising.sit`;
2. StuffIt method 15 / Arsenic decode of embedded `Deimos Rising 1.0.6.smi`;
3. exact SMI data fork length: **81,788,928 bytes**;
4. HFS catalog extraction of the `Deimos Rising` application's data fork;
5. exact PowerPC PEF length: **2,045,976 bytes**;
6. PEF prefix: `Joy!peffpwpc`;
7. PEF SHA-256: `8e436c3babc582f1407ae6fed47e9749f1c930335ce4c794947e40b06b85eb29`.

The recovered executable is research evidence only and is not distributed in source
snapshots.

## PPC routines closed in this pass

- `0x146F0` — state entry and explicit flee-ID latch handling;
- `0x15280` — player-target / no-player / Hunt / range / convergence dispatcher;
- `0x161C0` — current visual heading used by rotated child spawns;
- `0x16CC0` — flee acceleration;
- `0x17510` — FourCC flee destination initializer;
- `0x15B40` — entity spawn scheduler/geometry, re-audited for enemy firing;
- `0x17CB0` — state-entry spawn-runtime initialization, re-audited for cadence.

## Corrected flee semantics

WIP8 had a compatibility placeholder that accelerated away from the stored player point.
The original instead treats live `+0x11C/+0x120` as an authored flee **destination** and
accelerates toward it using `stateFleeSpeed_FLOAT` / `stateFleeDelta_FLOAT`.

`stateFlee_ID` is executable state data. Supported modes recovered from `0x17510` are:

| ID | Recovered target |
|---|---|
| `nora` | random X, north boundary |
| `sora` | random X, south boundary |
| `wera` | west boundary, random Y |
| `eara` | east boundary, random Y |
| `noce` | center X, north boundary |
| `soce` | center X, south boundary |
| `wece` | west boundary, center Y |
| `eace` | east boundary, center Y |
| `opve` | random X, opposite vertical edge |
| `opho` | opposite horizontal edge, random Y |
| `rave` | random north/south, then random X |
| `raho` | random east/west, then random Y |
| `cega` | visible game-area center |

Canonical `Game[gafl]` values are north=-1000, south=2000, west=-1000, east=2000,
width=416 and height=480.

State-entry RNG ordering is important: explicit flee-target randomization occurs before
`0x17CB0` consumes spawn-set rate/volley/delay RNG. A range transition into a flee state
installs the destination immediately but still performs ordinary convergence for the rest
of that dispatcher call; flee acceleration starts on the next tick.

The Unit Definition no-active-player flags are also live behavior: 8 canonical Units use
north flee and 1 uses south flee. They install `nora`/`sora` and return from the target
motion dispatcher for that tick.

## Current visual heading and enemy fire

WIP8 correctly kept `RotateToTarget` visual-only, but spawn geometry still supplied the
member's stale construction heading. PPC `0x161C0` proves rotation-adjusted spawn sets use
the current sprite frame and state direction geometry instead. WIP9 routes child-spawn
geometry through that helper.

The spawn scheduler itself did not need a new cadence heuristic. Re-disassembly of
`0x15B40`/`0x17CB0` reconfirmed the existing clean scheduler's strict countdown behavior
and RNG order. Enemy fire is authored as ordinary state spawn sets; the missing WIP8
piece was the live visual bearing used when a due spawn is rotation-adjusted.

## WIP8 -> WIP9 differential isolation

The same 3000-tick Level-1 stress was rebuilt four ways to isolate causes:

| Behavior | max resident | final resident | pruned | far-culls |
|---|---:|---:|---:|---:|
| WIP8 flee + stale heading | 96 | 27 | 1871 | 213 |
| WIP8 flee + WIP9 visual heading | 85 | 26 | 1655 | 152 |
| WIP9 authored flee + stale heading | 84 | 18 | 2219 | 246 |
| full WIP9 | 84 | 15 | 1773 | 136 |

Restoring both WIP8 behaviors reproduces the WIP8 oracle exactly. Therefore the WIP9
long-run population shift is completely accounted for by these two PPC-backed changes.
There is no unexplained third runtime change in this differential.

Early deterministic witnesses do not move: static frame hashes, live initial/fire/tick120
hashes, and the deliberate crash/respawn witness remain unchanged through WIP9.

## Full-WIP9 live diagnostic

The 3000-tick diagnostic records:

- 23 flee activations, all from explicit state flee modes in this scenario;
- 0 no-player flee activations in this particular input stream;
- 1,766 entity spawn-due events;
- 1,092 rotation-adjusted spawn events;
- 48 rotation-adjusted events where visual heading differs from construction heading.

These are clean-host diagnostic witnesses. They demonstrate that the heading correction
is exercised by canonical Level-1 content; they are not asserted as an original-executable
aggregate trace.

## Compatibility boundary

One state byte at compiled state `+0x349` participates in the non-flee branch surrounding
`0x16CC0`. No state in the canonical 386-unit corpus enables it. WIP9 intentionally leaves
that byte unnamed rather than assigning a guessed semantic. It therefore does not block
shipped 1.0.6 behavior closure.
