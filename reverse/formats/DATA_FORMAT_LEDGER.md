# Game-data format ledger

The canonical 1.0.6 game-data serialization is now substantially understood. Ten resource families share the same reversible legacy tagged-text encoding; `.film` is a separate fixed-size binary replay format.

| Extension | Count in Game.pak | Confidence | Meaning / status |
| --- | ---: | --- | --- |
| `.leve` | 12 | confirmed | typed clean loader implemented; 565 placements reconcile |
| `.unde` | 386 | confirmed serialization / strong semantics | unit/entity state-machine definitions; typed behavioral mapping underway |
| `.wede` | 5 | confirmed serialization/identity | weapon definitions |
| `.plde` | 2 | confirmed serialization/identity | player definitions |
| `.film` | 4 canonical + 11 Perfect Demos | partial binary layout confirmed | v10005 replay/input recordings; input bit semantics still being proven |
| `.idli` | 6 | confirmed | ID lists |
| `.flli` | 1 | confirmed | float list / global constants (`Game[gafl]`) |
| `.coli` | 1 | confirmed | color list (`Colors[gaco]`) |
| `.tefo` | 54 | confirmed | text-format definitions |
| `.stli` | 5 | confirmed | bare string lists |
| `.reli` | 1 | confirmed | rectangle list |

The clean core now contains the generic byte decoder/tagged-text parser, typed primitives, a strict level loader, a v10005 film parser for proven fields, and the original stored-ZIP PAK/Local provider layer.
