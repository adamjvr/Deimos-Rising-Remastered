# Unit / weapon / player data notes

Status: **serialization confirmed; behavioral semantics progressively mapped**.

The 386 `.unde` unit definitions are decoded tagged text, not compiled binary structs. A corpus scan finds 223 distinct keys. The files encode substantial behavior declaratively, including:

- group/spawn frequency and delay;
- player/projectile/terrain flags;
- shields, score, damage and hit behavior;
- state machines (`stateName_STR` and transition conditions/actions);
- sprite face/frame/direction animation;
- visibility, scale, tint/colorisation;
- movement/velocity/heading behavior;
- sound triggers;
- nested spawn sets;
- state rules with unit/range/condition/action fields.

Across the corpus there are 1,167 state records and 5,835 state-rule records based on repeated canonical key counts. This strongly supports a data-driven entity/state-machine architecture for the clean simulation.

The five `.wede` weapon resources and two `.plde` player resources use the same tagged-text encoding and similarly expose gameplay parameters directly. The next reconstruction step is to turn these proven field sequences into typed definitions while preserving unknown/less-certain semantics rather than guessing them.
