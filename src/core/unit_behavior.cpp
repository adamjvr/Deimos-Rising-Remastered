#include "deimos/unit_behavior.hpp"

namespace deimos {
namespace {
std::string_view field_string(const UnitStateDefinition& state, std::string_view key) {
    if (auto value = state.fields.string_value(key)) return *value;
    return {};
}
int field_int(const UnitStateDefinition& state, std::string_view key) {
    if (auto value = state.fields.int_value(key)) return *value;
    return 0;
}
float field_float(const UnitStateDefinition& state, std::string_view key) {
    if (auto value = state.fields.float_value(key)) return *value;
    return 0.0f;
}
bool field_bool(const UnitStateDefinition& state, std::string_view key) {
    if (auto value = state.fields.bool_value(key)) return *value;
    return false;
}
FourCC field_id(const UnitStateDefinition& state, std::string_view key) {
    if (auto value = state.fields.id_value(key)) return *value;
    return {};
}
bool core_bool(const UnitDefinition& unit, std::string_view key) {
    if (auto value = unit.core_fields.bool_value(key)) return *value;
    return false;
}
float core_float(const UnitDefinition& unit, std::string_view key) {
    if (auto value = unit.core_fields.float_value(key)) return *value;
    return 0.0f;
}
int core_int(const UnitDefinition& unit, std::string_view key) {
    if (auto value = unit.core_fields.int_value(key)) return *value;
    return 0;
}
FourCC core_id(const UnitDefinition& unit, std::string_view key) {
    if (auto value = unit.core_fields.id_value(key)) return *value;
    return {};
}
constexpr FourCC fourcc(char a, char b, char c, char d) {
    return FourCC{{a, b, c, d}};
}
}

UnitRuleConditionKind classify_unit_rule_condition(std::string_view condition) {
    if (condition.empty()) return UnitRuleConditionKind::unused;
    if (condition == "Is Tracking Player") return UnitRuleConditionKind::is_tracking_player;
    if (condition == "Is Not Tracking Player") return UnitRuleConditionKind::is_not_tracking_player;
    if (condition == "Is Active") return UnitRuleConditionKind::is_active;
    if (condition == "Is Not Active") return UnitRuleConditionKind::is_not_active;
    if (condition == "No Destroyable Air Entities Are Active") return UnitRuleConditionKind::no_destroyable_air_entities_active;
    if (condition == "No Destroyable Ground Entities Are Active") return UnitRuleConditionKind::no_destroyable_ground_entities_active;
    if (condition == "No Destroyable Air or Ground Entities Are Active") return UnitRuleConditionKind::no_destroyable_air_or_ground_entities_active;
    if (condition == "No Players Are Active") return UnitRuleConditionKind::no_players_active;
    if (condition == "This Entity is Within Range of a Player") return UnitRuleConditionKind::entity_within_range_of_player;
    if (condition == "This Entity is Not Within Range of a Player") return UnitRuleConditionKind::entity_not_within_range_of_player;
    if (condition == "This Entity's Animation Has Stopped") return UnitRuleConditionKind::animation_has_stopped;
    if (condition == "This Entity's Visibility is at Required Level") return UnitRuleConditionKind::visibility_at_required_level;
    if (condition == "This Entity's Tint is at Required Level") return UnitRuleConditionKind::tint_at_required_level;
    if (condition == "This Entity's Scale is at Required Level") return UnitRuleConditionKind::scale_at_required_level;
    if (condition == "Number of This Type of Entity Active") return UnitRuleConditionKind::number_of_this_type_active;
    if (condition == "Are Fewer of These Entities Active") return UnitRuleConditionKind::fewer_of_these_entities_active;
    if (condition == "Are More of These Entities Active") return UnitRuleConditionKind::more_of_these_entities_active;
    return UnitRuleConditionKind::unknown;
}

ResolvedStateAction resolve_state_action(const UnitDefinition& unit, std::string_view action) {
    ResolvedStateAction result;
    result.original_label = std::string(action);
    if (action.empty() || action == "No State") return result;
    if (action == "Delete") { result.kind = StateActionKind::delete_entity; return result; }
    if (action == "Destroy") { result.kind = StateActionKind::destroy_entity; return result; }
    if (const auto state = unit.find_state(action)) {
        result.kind = StateActionKind::change_state;
        result.state_index = *state;
        return result;
    }
    result.kind = StateActionKind::unresolved;
    return result;
}

CompiledUnitBehavior compile_unit_behavior(const UnitDefinition& unit) {
    CompiledUnitBehavior out;
    out.collision_domain = core_bool(unit, "isGroundBased_BOOL")
        ? fourcc('g', 'r', 'n', 'd')
        : fourcc('a', 'i', 'r', ' ');
    out.harmless_to_players = core_bool(unit, "harmlessToPlayers_BOOL");
    out.player_projectile = core_bool(unit, "playerProjectile_BOOL");
    out.can_be_hit_by_player_projectile = core_bool(unit, "canBeHitByPlayerProjectile_BOOL");
    out.hittable_when_invisible = core_bool(unit, "hittableWhenInvisible_BOOL");
    out.collision_damage = core_float(unit, "damage_FLOAT");
    out.shields_base = core_float(unit, "shields_BaseAmount_FLOAT");
    out.shields_level_increment = core_float(unit, "shields_LevelIncrement_FLOAT");
    out.shields_max = core_float(unit, "shields_MaxAmount_FLOAT");
    out.hit_particles = core_id(unit, "hitParticles_ID");
    out.score = core_int(unit, "score_INT");
    // PPC player-impact helper 0x37580 dispatches UnitDef +0x4D4 as a
    // pickup category and reads +0x4DC as its value. Loader/source correlation
    // binds those fields to pickup_Type_ID / pickup_Value_INT.
    out.pickup_type = core_id(unit, "pickup_Type_ID");
    out.pickup_value = core_int(unit, "pickup_Value_INT");
    out.states.reserve(unit.states.size());
    for (const auto& state : unit.states) {
        CompiledUnitStateBehavior compiled;
        compiled.range = field_float(state, "stateOnRange_FLOAT");
        compiled.on_range = resolve_state_action(unit, field_string(state, "stateOnRangeChangeTo_STR"));
        compiled.pass_hits_to_owner = field_bool(state, "passHitsToOwner_BOOL");
        compiled.collides = field_bool(state, "stateCollides_BOOL");
        compiled.invulnerable_on_collision = field_bool(
            state, "stateInvulnerable_ShieldsDoNotDepleteOnCollision_BOOL");
        compiled.collides_with_players = field_bool(state, "stateCollidesWithPlayers_BOOL");
        compiled.do_not_glow_on_collision = field_bool(state, "stateDoNotGlowOnCollision_BOOL");
        compiled.collision_spawn = field_id(state, "collision_Spawn_ID");
        compiled.collision_repeat_spawns = field_bool(state, "collision_RepeatSpawns_BOOL");
        compiled.collision_spawn_delay = field_int(state, "collision_SpawnDelay_INT");
        compiled.on_hit = resolve_state_action(unit, field_string(state, "stateOnHitChangeTo_STR"));
        compiled.hit_state_delay = field_int(state, "stateOnHitChangeStateDelay_INT");
        compiled.timer_min = field_int(state, "stateOnTimerMin_INT");
        compiled.timer_max = field_int(state, "stateOnTimerMax_INT");
        compiled.on_timer = resolve_state_action(unit, field_string(state, "stateOnTimerChangeTo_STR"));
        compiled.counter = field_int(state, "stateOnCounter_INT");
        compiled.on_counter = resolve_state_action(unit, field_string(state, "stateOnCounterChangeTo_STR"));
        compiled.rules.reserve(state.rules.size());
        for (const auto& rule : state.rules) {
            compiled.rules.push_back({
                classify_unit_rule_condition(rule.condition), rule.unit_id, rule.range,
                resolve_state_action(unit, rule.action)
            });
        }

        const auto count_unresolved = [&](const ResolvedStateAction& action, bool active) {
            if (action.kind != StateActionKind::unresolved) return;
            if (active) ++out.unresolved_active_actions;
            else ++out.unresolved_inert_actions;
        };
        // The original range check treats exactly zero as disabled.  It does
        // not require the threshold to be positive.
        count_unresolved(compiled.on_range, compiled.range != 0.0f);
        count_unresolved(compiled.on_hit, !compiled.on_hit.original_label.empty());
        count_unresolved(compiled.on_timer, (compiled.timer_min != 0 || compiled.timer_max != 0));
        count_unresolved(compiled.on_counter, compiled.counter > 0);
        for (const auto& rule : compiled.rules) {
            const bool active = rule.condition != UnitRuleConditionKind::unused && !rule.action.original_label.empty();
            count_unresolved(rule.action, active);
        }
        out.states.push_back(std::move(compiled));
    }
    return out;
}

bool evaluate_unit_rule_condition(
    UnitRuleConditionKind condition,
    const UnitRuleFacts& facts,
    int rule_range) {
    switch (condition) {
    case UnitRuleConditionKind::unused:
    case UnitRuleConditionKind::unknown:
        return false;
    case UnitRuleConditionKind::is_tracking_player:
        return facts.tracking_player;
    case UnitRuleConditionKind::is_not_tracking_player:
        return !facts.tracking_player;
    case UnitRuleConditionKind::is_active:
        return facts.active;
    case UnitRuleConditionKind::is_not_active:
        return !facts.active;
    case UnitRuleConditionKind::no_destroyable_air_entities_active:
        return !facts.destroyable_air_entities_active;
    case UnitRuleConditionKind::no_destroyable_ground_entities_active:
        return !facts.destroyable_ground_entities_active;
    case UnitRuleConditionKind::no_destroyable_air_or_ground_entities_active:
        return !facts.destroyable_air_entities_active && !facts.destroyable_ground_entities_active;
    case UnitRuleConditionKind::no_players_active:
        return !facts.players_active;
    case UnitRuleConditionKind::entity_within_range_of_player:
        // PPC 0x157cc skips the range query entirely when rule.range == 0,
        // leaving the result false.
        return rule_range != 0 && facts.entity_within_player_range;
    case UnitRuleConditionKind::entity_not_within_range_of_player:
        // PPC 0x157f0 has the same zero-range early-out.  Therefore this is
        // deliberately *not* simply !within when range == 0.
        return rule_range != 0 && !facts.entity_within_player_range;
    case UnitRuleConditionKind::animation_has_stopped:
        return facts.animation_stopped;
    case UnitRuleConditionKind::visibility_at_required_level:
        return facts.visibility == facts.required_visibility;
    case UnitRuleConditionKind::tint_at_required_level:
        return facts.tint == facts.required_tint;
    case UnitRuleConditionKind::scale_at_required_level:
        return facts.scale == facts.required_scale;
    case UnitRuleConditionKind::number_of_this_type_active:
        return facts.matching_unit_active_count == rule_range;
    case UnitRuleConditionKind::fewer_of_these_entities_active:
        return facts.matching_unit_active_count < rule_range;
    case UnitRuleConditionKind::more_of_these_entities_active:
        return facts.matching_unit_active_count > rule_range;
    }
    return false;
}

RuleEvaluationResult evaluate_first_matching_rule(
    const CompiledUnitStateBehavior& state,
    const UnitRuleFactsProvider& facts_for_rule) {
    for (std::size_t i = 0; i < state.rules.size(); ++i) {
        const auto& rule = state.rules[i];
        if (rule.condition == UnitRuleConditionKind::unused ||
            rule.condition == UnitRuleConditionKind::unknown) {
            continue;
        }
        const auto facts = facts_for_rule(rule, i);
        if (evaluate_unit_rule_condition(rule.condition, facts, rule.range)) {
            // PPC 0x158e4..0x15900 calls the state-action routine and then
            // exits the rule loop unconditionally.  An unresolved label is a
            // no-op action, but it still consumes this tick's rule decision.
            return {true, i, rule.action};
        }
    }
    return {};
}

} // namespace deimos
