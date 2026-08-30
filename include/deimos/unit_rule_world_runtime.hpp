#pragma once

#include "deimos/entity_world.hpp"
#include "deimos/unit_behavior.hpp"

namespace deimos {

// World-facing inputs sampled by PPC rule evaluator 0x15550 after animation
// processing and before the later player-target/range dispatcher.  The clean
// core keeps these queries separate from predicate dispatch so their spatial
// and sentinel semantics can be regression-tested independently.
struct UnitRuleWorldContext {
    const EntityWorld* entities = nullptr;
    const PlayerWorld* players = nullptr;
    EntityPoint subject_position{};

    // Current sprite/visual state at the rule-evaluation point. Canonical
    // 1.0.6 does not use the required-level predicates, but the executable
    // supports them and the values are already recovered by render_runtime.
    float visibility = 0.0f;
    float required_visibility = 0.0f;
    float tint = 0.0f;
    float required_tint = 0.0f;
    float scale = 0.0f;
    float required_scale = 0.0f;

    // PPC 0x15930 owns this bit. Keep it explicit until sprite-frame animation
    // is instruction-closed; callers must not infer it from visual scalars.
    bool animation_stopped = false;
};

// 0x15550's Is Active / Is Not Active helpers receive Unit ID, subject
// position and integer rule range. A zero range is the corpus' global-query
// form; a non-zero range filters matching live members spatially.
[[nodiscard]] bool legacy_rule_active_query(
    const EntityWorld& world,
    FourCC unit_id,
    EntityPoint subject_position,
    int range);

// Tracking uses the same Unit-ID/range-shaped world query, restricted to live
// matching members that currently own an active player target. In canonical
// 1.0.6 every Is Tracking Player slot carries sentinel Unit ID 'none', so this
// safely remains false for the 2,773 template/default tracking slots.
[[nodiscard]] bool legacy_rule_tracking_query(
    const EntityWorld& world,
    FourCC unit_id,
    EntityPoint subject_position,
    int range);

[[nodiscard]] int legacy_rule_active_count(
    const EntityWorld& world,
    FourCC unit_id);

[[nodiscard]] bool legacy_destroyable_entities_active(
    const EntityWorld& world,
    FourCC collision_domain);

// Build the pure UnitRuleFacts consumed by evaluate_first_matching_rule().
// This restores the live world's missing rule-query bridge without coupling
// the deterministic state interpreter to a specific host/game session.
[[nodiscard]] UnitRuleFacts build_unit_rule_world_facts(
    const CompiledStateRule& rule,
    const UnitRuleWorldContext& context);

} // namespace deimos
