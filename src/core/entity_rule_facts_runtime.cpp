#include "deimos/entity_rule_facts_runtime.hpp"

#include <cmath>

namespace deimos {
namespace {

constexpr FourCC fourcc(char a, char b, char c, char d) {
    return FourCC{{a,b,c,d}};
}

bool absent(FourCC id) {
    return id == FourCC{} || id == fourcc('n','o','n','e') || id == fourcc('N','U','L','L');
}

bool active(const EntityRuntime& entity) {
    return entity.lifecycle == EntityLifecycle::active;
}

float legacy_distance(float x1, float y1, float x2, float y2) {
    const float dx = static_cast<float>(x2 - x1);
    const float dy = static_cast<float>(y2 - y1);
    const float squared = std::fma(dx, dx, static_cast<float>(dy * dy));
    const int squared_integer = static_cast<int>(std::trunc(squared));
    if (squared_integer <= 0) return 0.0f;
    return static_cast<float>(std::sqrt(static_cast<float>(squared_integer)));
}

bool within_rule_range(const EntityRuntime& self, const EntityRuntime& candidate, int range) {
    if (range == 0) return true;
    return legacy_distance(self.x, self.y, candidate.x, candidate.y) < static_cast<float>(range);
}

} // namespace

UnitRuleFacts build_entity_rule_facts(
    const EntityWorld& world,
    const PlayerWorld& players,
    const EntityRuntime& self,
    const CompiledStateRule& rule,
    const EntityRuleVisualFacts& visual) {
    UnitRuleFacts facts;
    facts.players_active = players.any_active_player();
    facts.animation_stopped = visual.animation_stopped;
    facts.visibility = visual.visibility;
    facts.required_visibility = visual.required_visibility;
    facts.tint = visual.tint;
    facts.required_tint = visual.required_tint;
    facts.scale = visual.scale;
    facts.required_scale = visual.required_scale;

    if (const auto closest = players.closest_active_player(self.x, self.y)) {
        if (rule.range != 0) {
            facts.entity_within_player_range =
                closest->distance < static_cast<float>(rule.range);
        }
    }

    for (const auto& candidate : world.members()) {
        if (!active(candidate)) continue;

        if (candidate.behavior.can_be_hit_by_player_projectile) {
            if (candidate.behavior.collision_domain == fourcc('a','i','r',' ')) {
                facts.destroyable_air_entities_active = true;
            } else if (candidate.behavior.collision_domain == fourcc('g','r','n','d')) {
                facts.destroyable_ground_entities_active = true;
            }
        }

        if (absent(rule.unit_id) || candidate.unit_id != rule.unit_id) continue;
        ++facts.matching_unit_active_count;
        if (!within_rule_range(self, candidate, rule.range)) continue;
        facts.active = true;
        if (candidate.has_active_target) facts.tracking_player = true;
    }

    return facts;
}

} // namespace deimos
