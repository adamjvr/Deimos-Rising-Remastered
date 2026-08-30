#include "deimos/unit_rule_world_runtime.hpp"

#include <cmath>
#include <limits>

namespace deimos {
namespace {

constexpr FourCC kAirDomain{{'a','i','r',' '}};
constexpr FourCC kGroundDomain{{'g','r','n','d'}};

bool present_unit_id(FourCC id) {
    const auto s = id.str();
    return !(id == FourCC{}) && s != "none" && s != "NULL";
}

float legacy_rule_distance(EntityPoint a, EntityPoint b) {
    // The world/player geometry family in 1.0.6 performs the displacement and
    // square in single precision, truncates squared distance to an integer,
    // then square-roots it. Preserve those recovered rounding gates here.
    const float dx = static_cast<float>(b.x - a.x);
    const float dy = static_cast<float>(b.y - a.y);
    const float squared = std::fma(dx, dx, static_cast<float>(dy * dy));
    const int quantized_squared = static_cast<int>(std::trunc(squared));
    if (quantized_squared <= 0) return 0.0f;
    return static_cast<float>(std::sqrt(static_cast<float>(quantized_squared)));
}

template <class Predicate>
bool matching_member_query(
    const EntityWorld& world,
    FourCC unit_id,
    EntityPoint subject_position,
    int range,
    Predicate&& predicate) {
    if (!present_unit_id(unit_id)) return false;
    for (const auto& member : world.members()) {
        if (member.lifecycle != EntityLifecycle::active || member.unit_id != unit_id) continue;
        if (!predicate(member)) continue;
        if (range == 0) return true;
        if (legacy_rule_distance(subject_position, {member.x, member.y}) < static_cast<float>(range)) {
            return true;
        }
    }
    return false;
}

} // namespace

bool legacy_rule_active_query(
    const EntityWorld& world,
    FourCC unit_id,
    EntityPoint subject_position,
    int range) {
    return matching_member_query(
        world, unit_id, subject_position, range,
        [](const EntityRuntime&) { return true; });
}

bool legacy_rule_tracking_query(
    const EntityWorld& world,
    FourCC unit_id,
    EntityPoint subject_position,
    int range) {
    return matching_member_query(
        world, unit_id, subject_position, range,
        [](const EntityRuntime& member) { return member.has_active_target; });
}

int legacy_rule_active_count(const EntityWorld& world, FourCC unit_id) {
    if (!present_unit_id(unit_id)) return 0;
    int count = 0;
    for (const auto& member : world.members()) {
        if (member.lifecycle == EntityLifecycle::active && member.unit_id == unit_id) ++count;
    }
    return count;
}

bool legacy_destroyable_entities_active(const EntityWorld& world, FourCC collision_domain) {
    if (collision_domain != kAirDomain && collision_domain != kGroundDomain) return false;
    for (const auto& member : world.members()) {
        if (member.lifecycle != EntityLifecycle::active) continue;
        if (member.behavior.collision_domain != collision_domain) continue;
        if (!member.behavior.can_be_hit_by_player_projectile) continue;
        return true;
    }
    return false;
}

UnitRuleFacts build_unit_rule_world_facts(
    const CompiledStateRule& rule,
    const UnitRuleWorldContext& context) {
    UnitRuleFacts facts;
    facts.visibility = context.visibility;
    facts.required_visibility = context.required_visibility;
    facts.tint = context.tint;
    facts.required_tint = context.required_tint;
    facts.scale = context.scale;
    facts.required_scale = context.required_scale;
    facts.animation_stopped = context.animation_stopped;

    if (context.entities) {
        facts.tracking_player = legacy_rule_tracking_query(
            *context.entities, rule.unit_id, context.subject_position, rule.range);
        facts.active = legacy_rule_active_query(
            *context.entities, rule.unit_id, context.subject_position, rule.range);
        facts.matching_unit_active_count = legacy_rule_active_count(*context.entities, rule.unit_id);
        facts.destroyable_air_entities_active = legacy_destroyable_entities_active(
            *context.entities, kAirDomain);
        facts.destroyable_ground_entities_active = legacy_destroyable_entities_active(
            *context.entities, kGroundDomain);
    }

    if (context.players) {
        facts.players_active = context.players->any_active_player();
        if (rule.range != 0) {
            if (const auto closest = context.players->closest_active_player(
                    context.subject_position.x, context.subject_position.y)) {
                facts.entity_within_player_range =
                    closest->distance < static_cast<float>(rule.range);
            }
        }
    }
    return facts;
}

} // namespace deimos
