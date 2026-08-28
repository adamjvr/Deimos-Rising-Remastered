# Game-data format ledger

| Extension | Count in Game.pak | Current confidence | Meaning / next step |
| --- | ---: | --- | --- |
| `.leve` | 12 | confirmed identity | levels 01–12; decode object lists, terrain/background, sequence/timing |
| `.unde` | 386 | strong | unit/entity definitions; correlate with sprite/sound/weapon tags and source strings |
| `.wede` | 5 | confirmed identity | Bacta Gun, Ion Cannon, Photon Beam, Rear Gun, Plasma Bomb |
| `.plde` | 2 | confirmed identity | player 1/2 definitions |
| `.film` | 4 | strong | built-in demo/replay recordings; add-on corpus expands this dramatically |
| `.idli` | 6 | strong | Editor, Fonts, Formats, Objects, Sounds, Sprites ID lists |
| `.flli` | 1 | tentative structure | global/game list (`Game[gafl]`) |
| `.coli` | 1 | confirmed identity | color table (`Colors[gaco]`) |
| `.tefo` | 54 | tentative | decode by cross-references and image/level use |
| `.stli` | 5 | tentative | decode by field-key/source-string correspondence |
| `.reli` | 1 | tentative | decode by field-key/source-string correspondence |

The binary data files share recurring signatures/structure but are not yet semantically decoded. The next milestone is a field-level parser, not a guessed struct dump.
