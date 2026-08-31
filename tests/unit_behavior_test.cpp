#include "deimos/unit_behavior.hpp"

#include <cassert>
#include <limits>
#include <string_view>
#include <utility>
#include <vector>

int main() {
    deimos::UnitDefinition unit;
    unit.states.resize(2);
    unit.states[0].name = "Wait For Player Approach";
    unit.states[1].name = "Attack";

    auto action = deimos::resolve_state_action(unit, "Wait For Player Approach");
    assert(action.kind == deimos::StateActionKind::change_state);
    assert(action.state_index == 0);
    // Case-only mismatches are unresolved in the original byte-exact lookup.
    assert(deimos::resolve_state_action(unit, "Wait for Player Approach").kind ==
           deimos::StateActionKind::unresolved);
    assert(deimos::resolve_state_action(unit, "Delete").kind == deimos::StateActionKind::delete_entity);
    assert(deimos::resolve_state_action(unit, "Destroy").kind == deimos::StateActionKind::destroy_entity);
    assert(deimos::resolve_state_action(unit, "No State").kind == deimos::StateActionKind::none);
    const auto unresolved = deimos::resolve_state_action(unit, "Missing");
    assert(unresolved.kind == deimos::StateActionKind::unresolved);
    assert(unresolved.is_runtime_noop());

    using K = deimos::UnitRuleConditionKind;
    const std::vector<std::pair<std::string_view, K>> conditions = {
        {"", K::unused},
        {"Is Tracking Player", K::is_tracking_player},
        {"Is Not Tracking Player", K::is_not_tracking_player},
        {"Is Active", K::is_active},
        {"Is Not Active", K::is_not_active},
        {"No Destroyable Air Entities Are Active", K::no_destroyable_air_entities_active},
        {"No Destroyable Ground Entities Are Active", K::no_destroyable_ground_entities_active},
        {"No Destroyable Air or Ground Entities Are Active", K::no_destroyable_air_or_ground_entities_active},
        {"No Players Are Active", K::no_players_active},
        {"This Entity is Within Range of a Player", K::entity_within_range_of_player},
        {"This Entity is Not Within Range of a Player", K::entity_not_within_range_of_player},
        {"This Entity's Animation Has Stopped", K::animation_has_stopped},
        {"This Entity's Visibility is at Required Level", K::visibility_at_required_level},
        {"This Entity's Tint is at Required Level", K::tint_at_required_level},
        {"This Entity's Scale is at Required Level", K::scale_at_required_level},
        {"Number of This Type of Entity Active", K::number_of_this_type_active},
        {"Are Fewer of These Entities Active", K::fewer_of_these_entities_active},
        {"Are More of These Entities Active", K::more_of_these_entities_active},
    };
    for (const auto& [text, expected] : conditions) {
        assert(deimos::classify_unit_rule_condition(text) == expected);
    }
    assert(deimos::classify_unit_rule_condition("future mod condition") == K::unknown);

    deimos::UnitRuleFacts facts;
    facts.tracking_player = true;
    facts.active = true;
    facts.destroyable_air_entities_active = false;
    facts.destroyable_ground_entities_active = false;
    facts.players_active = false;
    facts.entity_within_player_range = true;
    facts.animation_stopped = true;
    facts.visibility = 0.5f;
    facts.required_visibility = 0.5f;
    facts.tint = 0.25f;
    facts.required_tint = 0.25f;
    facts.scale = 2.0f;
    facts.required_scale = 2.0f;
    facts.matching_unit_active_count = 3;

    assert(deimos::evaluate_unit_rule_condition(K::is_tracking_player, facts, 0));
    assert(!deimos::evaluate_unit_rule_condition(K::is_not_tracking_player, facts, 0));
    assert(deimos::evaluate_unit_rule_condition(K::is_active, facts, 0));
    assert(!deimos::evaluate_unit_rule_condition(K::is_not_active, facts, 0));
    assert(deimos::evaluate_unit_rule_condition(K::no_destroyable_air_entities_active, facts, 0));
    assert(deimos::evaluate_unit_rule_condition(K::no_destroyable_ground_entities_active, facts, 0));
    assert(deimos::evaluate_unit_rule_condition(K::no_destroyable_air_or_ground_entities_active, facts, 0));
    assert(deimos::evaluate_unit_rule_condition(K::no_players_active, facts, 0));
    assert(deimos::evaluate_unit_rule_condition(K::entity_within_range_of_player, facts, 10));
    assert(!deimos::evaluate_unit_rule_condition(K::entity_not_within_range_of_player, facts, 10));
    // Exact original quirk: range zero makes both range predicates false.
    assert(!deimos::evaluate_unit_rule_condition(K::entity_within_range_of_player, facts, 0));
    assert(!deimos::evaluate_unit_rule_condition(K::entity_not_within_range_of_player, facts, 0));
    assert(deimos::evaluate_unit_rule_condition(K::animation_has_stopped, facts, 0));
    assert(deimos::evaluate_unit_rule_condition(K::visibility_at_required_level, facts, 0));
    assert(deimos::evaluate_unit_rule_condition(K::tint_at_required_level, facts, 0));
    assert(deimos::evaluate_unit_rule_condition(K::scale_at_required_level, facts, 0));
    assert(deimos::evaluate_unit_rule_condition(K::number_of_this_type_active, facts, 3));
    assert(deimos::evaluate_unit_rule_condition(K::fewer_of_these_entities_active, facts, 4));
    assert(deimos::evaluate_unit_rule_condition(K::more_of_these_entities_active, facts, 2));
    assert(!deimos::evaluate_unit_rule_condition(K::unknown, facts, 0));

    // WIP8 animation/orientation/scroll-state compiler fields are source data,
    // not host-side presentation guesses. Freeze the exact field names here.
    deimos::UnitDefinition animated_unit;
    animated_unit.states.resize(1);
    animated_unit.states[0].name = "Animated";
    auto add = [&](const char* key, deimos::DefinitionValue value, const char* raw) {
        animated_unit.states[0].fields.add({key, std::move(value), raw, 1});
    };
    add("stateDoAnimateBackwards_BOOL", true, "TRUE");
    add("stateDoLoopAnimation_BOOL", true, "TRUE");
    add("stateContinuousFrameRandomisation_BOOL", true, "TRUE");
    add("stateDoRotateToTarget_BOOL", true, "TRUE");
    add("stateNumDirections_INT", 24, "24");
    add("stateSpriteFrameMin_INT", 3, "3");
    add("stateSpriteFrameMax_INT", 7, "7");
    add("stateFramesPerDirection_INT", 8, "8");
    add("stateFrameDelay_INT", 2, "2");
    add("stateFrameDelta_INT", 1, "1");
    add("stateUseParentDirection_BOOL", true, "TRUE");
    add("statePauseVerticalScrolling_BOOL", true, "TRUE");
    const auto animated = deimos::compile_unit_behavior(animated_unit);
    assert(animated.states.size() == 1);
    const auto& a = animated.states[0];
    assert(a.animate_backwards && a.loop_animation &&
           a.continuous_frame_randomisation && a.rotate_to_target);
    assert(a.number_of_directions == 24 && a.sprite_frame_min == 3 &&
           a.sprite_frame_max == 7 && a.frames_per_direction == 8 &&
           a.frame_delay == 2 && a.frame_delta == 1);
    assert(a.use_parent_direction && a.pause_vertical_scrolling);

    // PPC fcmpu equality is exact.  NaN must not count as "at required level".
    facts.visibility = std::numeric_limits<float>::quiet_NaN();
    facts.required_visibility = facts.visibility;
    assert(!deimos::evaluate_unit_rule_condition(K::visibility_at_required_level, facts, 0));

    // File-order/first-true semantics: rule 0 has an unresolved action but is
    // true, so rule 1 must never be queried or selected.
    deimos::CompiledUnitStateBehavior state;
    state.rules.push_back({K::is_active, {}, 0, unresolved});
    state.rules.push_back({K::no_players_active, {}, 0,
                           deimos::resolve_state_action(unit, "Attack")});
    std::size_t provider_calls = 0;
    const auto result = deimos::evaluate_first_matching_rule(
        state,
        [&](const deimos::CompiledStateRule&, std::size_t index) {
            ++provider_calls;
            deimos::UnitRuleFacts f;
            if (index == 0) f.active = true;
            if (index == 1) f.players_active = false;
            return f;
        });
    assert(result.matched);
    assert(result.rule_index == 0);
    assert(result.action.kind == deimos::StateActionKind::unresolved);
    assert(provider_calls == 1);

    return 0;
}
