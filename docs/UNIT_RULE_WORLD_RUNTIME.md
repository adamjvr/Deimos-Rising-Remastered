# Unit-rule world facts runtime — Mac 1.0.6

Status: **recovered rule evaluator connected to live world; lower spatial helper remains bounded**.

The entity state runtime already mirrors PPC `0x15550` and evaluates up to five
serialized rules in order. The original live preview accidentally provided no
`EntityTickContext::facts_for_rule`, so every world-dependent rule slot was
skipped. `UnitRuleWorldRuntime` supplies those facts without changing the
recovered rule evaluator itself.

## Supplied facts

The bridge derives:

- active Unit-ID/range query;
- tracking Unit-ID/range query;
- active count for a Unit ID;
- any players active;
- closest active player distance/range;
- no player-projectile-hittable ground entities active;
- no player-projectile-hittable air entities active;
- current/required visibility, tint and scale.

Sentinel IDs (`none`, `NULL`, empty) never manufacture an active/tracking match.
Range zero is the unbounded Unit-ID query; nonzero ranges use the isolated clean
quantized-distance helper pending exact lower-level PPC closure.

## Canonical corpus guardrail

Across stock Mac 1.0.6 Unit Definitions there are 5,835 rule slots. The most
important audit result is that all **2,773** `Is Tracking Player` slots carry
Unit ID `none`. They are default/inert slots. Feeding the member's later
`has_active_target` flag into these rules would incorrectly make thousands of
Delete transitions eligible.

Other canonical conditions do contain meaningful non-sentinel IDs, including
`Is Active`, `Is Not Active`, and active-count comparisons, so the world query
provider is required even though the tracking template slots are inert.

`Animation Stopped` is intentionally false in the live provider until sprite
animation timing is instruction-closed; visual scalar comparison facts are
wired from the current `LegacySpriteVisualRuntime`.
