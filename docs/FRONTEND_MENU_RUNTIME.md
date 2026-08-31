# Front-End / Menu Runtime

Status: **native macOS front-end restored from 1.0.6 resource evidence; exact legacy artwork/dialog renderer remains future work**.

## Original 1.0.6 resource evidence

The recovered application resource fork (PEF data-fork SHA-256 `8e436c3babc582f1407ae6fed47e9749f1c930335ce4c794947e40b06b85eb29`) contains the following front-end resources:

- `MBAR 128` — `Interface MBAR`;
- `MENU 128` — `About`, including `About Deimos Rising`;
- `MENU 2000` — `Interface - File`, containing `Quit`;
- `MENU 2001` — `Interface - Edit`, containing Undo/Cut/Copy/Paste/Clear;
- `DLOG/DITL 190` — `Game - Preferences`;
- `DLOG/DITL 191` — `Game - Controls`;
- `DLOG/DITL 192` — `Game - HID Controls`;
- `DLOG/DITL 193` — `Game - HID Calibration`;
- `STR# 130` — 128-entry `Game Keys` key-name table;
- `tset 493` / `tset 2558` — `Deimos Default Keyboard`.

The original Preferences dialog text exposes Full Screen, Interlacing, Bypass System Volume, Music Volume, Sound Volume, ESC Key Delay, Set Controls, and Set Gamepad Controls. The original keyboard set contains fourteen bindings: two seven-action player sets. Player 1 begins with key codes `0x7B,0x7C,0x7D,0x7E,0x3A,0x37,0x31`, corresponding to Left, Right, Down, Up, Option, Command, Space. The Controls dialog labels the final three actions `Air`, `Ground`, and `Select`, establishing the recovered Player-1 defaults:

- Arrow Keys — movement;
- Option — Fire Air Weapon;
- Command — Fire Ground Weapon;
- Space — Select / cycle weapon.

## Current native host

The macOS playable host now restores a discoverable front-end rather than immediately entering combat:

- launch menu: Start Level / Controls / Preferences / Quit;
- Game menu: Pause / Resume, Restart Level, Controls;
- View menu: Toggle Full Screen;
- Help menu: controls quick reference;
- Escape opens a real pause menu with Resume / Controls / Preferences / Restart Level;
- simulation input is cleared while modal front-end UI is active;
- the 30 Hz gameplay loop does not advance while paused.

The recovered original defaults are accepted alongside modern aliases:

| Action | Original 1.0.6 | Modern aliases |
| --- | --- | --- |
| Move | Arrow Keys | WASD |
| Air weapon | Option | Z |
| Ground weapon | Command | X / Shift |
| Select / cycle air weapon | Space | C / Tab |
| Pause | host Escape menu | Escape |

The ground-weapon targeting reticle remains the PPC-closed `pbta` normal/locked presentation from WIP6.

## Evidence boundary

This milestone restores the original front-end *structure and control contract* using recovered resource evidence, but it does not claim pixel-identical reconstruction of the classic Appearance Manager dialogs. Modern AppKit menus/alerts are used as the host presentation until the full legacy DLOG/DITL/PICT rendering path is worth reproducing. Audio/gamepad preference widgets are described but not falsely enabled before their runtimes exist.
