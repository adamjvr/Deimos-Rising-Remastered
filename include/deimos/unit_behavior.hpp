#pragma once

#include "deimos/unit_definition.hpp"

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace deimos {

// The 1.0.6 PPC rule evaluator at code offset 0x15550 owns a 17-entry
// condition dispatch table.  Keep this list complete even though the shipped
// 1.0.6 data only exercises a subset: older/newer content can still target
// conditions that the executable supports.
enum class UnitRuleConditionKind {
    unused,
    is_tracking_player,
    is_not_tracking_player,
    is_active,
    is_not_active,
    no_destroyable_air_entities_active,
    no_destroyable_ground_entities_active,
    no_destroyable_air_or_ground_entities_active,
    no_players_active,
    entity_within_range_of_player,
    entity_not_within_range_of_player,
    animation_has_stopped,
    visibility_at_required_level,
    tint_at_required_level,
    scale_at_required_level,
    number_of_this_type_active,
    fewer_of_these_entities_active,
    more_of_these_entities_active,
    unknown
};

enum class StateActionKind {
    none,
    change_state,
    delete_entity,
    destroy_entity,
    // The original state-action routine silently returns if a non-empty label
    // is not Delete/Destroy and does not match a local state.  Preserve the
    // label for evidence/debugging, but runtime execution is a no-op.
    unresolved
};

struct ResolvedStateAction {
    StateActionKind kind = StateActionKind::none;
    std::size_t state_index = 0;
    std::string original_label;

    [[nodiscard]] bool is_runtime_noop() const {
        return kind == StateActionKind::none || kind == StateActionKind::unresolved;
    }
};

struct CompiledStateRule {
    UnitRuleConditionKind condition = UnitRuleConditionKind::unused;
    FourCC unit_id{};
    int range = 0;
    ResolvedStateAction action;
};

struct CompiledUnitStateBehavior {
    float range = 0.0f;
    ResolvedStateAction on_range;
    ResolvedStateAction on_hit;
    int hit_state_delay = 0;
    int timer_min = 0;
    int timer_max = 0;
    ResolvedStateAction on_timer;
    int counter = 0;
    ResolvedStateAction on_counter;
    std::vector<CompiledStateRule> rules;
};

struct CompiledUnitBehavior {
    std::vector<CompiledUnitStateBehavior> states;
    std::size_t unresolved_active_actions = 0;
    std::size_t unresolved_inert_actions = 0;
};

// World/query results consumed by one rule predicate.  The original evaluator
// calls world functions using the rule's Unit ID/range; the clean simulation
// will populate these facts from its world model.  Keeping predicate logic
// pure makes the binary-confirmed comparison semantics independently testable.
struct UnitRuleFacts {
    bool tracking_player = false;
    bool active = false;
    bool destroyable_air_entities_active = false;
    bool destroyable_ground_entities_active = false;
    bool players_active = false;
    bool entity_within_player_range = false;
    bool animation_stopped = false;

    float visibility = 0.0f;
    float required_visibility = 0.0f;
    float tint = 0.0f;
    float required_tint = 0.0f;
    float scale = 0.0f;
    float required_scale = 0.0f;

    int matching_unit_active_count = 0;
};

struct RuleEvaluationResult {
    bool matched = false;
    std::size_t rule_index = 0;
    ResolvedStateAction action;
};

using UnitRuleFactsProvider =
    std::function<UnitRuleFacts(const CompiledStateRule&, std::size_t rule_index)>;

UnitRuleConditionKind classify_unit_rule_condition(std::string_view condition);
ResolvedStateAction resolve_state_action(const UnitDefinition& unit, std::string_view action);
CompiledUnitBehavior compile_unit_behavior(const UnitDefinition& unit);

// PPC-confirmed predicate semantics from the 17-way dispatch in 0x15550.
[[nodiscard]] bool evaluate_unit_rule_condition(
    UnitRuleConditionKind condition,
    const UnitRuleFacts& facts,
    int rule_range);

// The original scans the five slots in file order and stops after the first
// true condition even when that rule's action label resolves to a no-op.
[[nodiscard]] RuleEvaluationResult evaluate_first_matching_rule(
    const CompiledUnitStateBehavior& state,
    const UnitRuleFactsProvider& facts_for_rule);

} // namespace deimos
