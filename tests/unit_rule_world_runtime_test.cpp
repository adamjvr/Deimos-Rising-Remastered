#include "deimos/unit_rule_world_runtime.hpp"

#include <cassert>

namespace {
deimos::EntityRuntime make_member(
    deimos::EntityHandle handle,
    deimos::FourCC id,
    float x,
    float y,
    deimos::FourCC domain,
    bool destroyable,
    bool tracking = false) {
    deimos::EntityRuntime out;
    out.handle = handle;
    out.serial = static_cast<std::uint32_t>(handle);
    out.unit_id = id;
    out.x = x;
    out.y = y;
    out.lifecycle = deimos::EntityLifecycle::active;
    out.behavior.collision_domain = domain;
    out.behavior.can_be_hit_by_player_projectile = destroyable;
    out.has_active_target = tracking;
    return out;
}
}

int main() {
    constexpr deimos::FourCC kFoo{{'f','o','o','1'}};
    constexpr deimos::FourCC kBar{{'b','a','r','1'}};
    constexpr deimos::FourCC kAir{{'a','i','r',' '}};
    constexpr deimos::FourCC kGround{{'g','r','n','d'}};
    constexpr deimos::FourCC kNone{{'n','o','n','e'}};

    deimos::EntityWorld world;
    world.members().push_back(make_member(1, kFoo, 10.0f, 0.0f, kAir, true, true));
    world.members().push_back(make_member(2, kFoo, 100.0f, 0.0f, kAir, true));
    world.members().push_back(make_member(3, kBar, 0.0f, 0.0f, kGround, false));

    assert(deimos::legacy_rule_active_query(world, kFoo, {0.0f, 0.0f}, 0));
    assert(deimos::legacy_rule_active_query(world, kFoo, {0.0f, 0.0f}, 11));
    assert(!deimos::legacy_rule_active_query(world, kFoo, {0.0f, 0.0f}, 10));
    assert(!deimos::legacy_rule_active_query(world, kNone, {0.0f, 0.0f}, 0));
    assert(deimos::legacy_rule_tracking_query(world, kFoo, {0.0f, 0.0f}, 11));
    assert(!deimos::legacy_rule_tracking_query(world, kFoo, {20.0f, 0.0f}, 10));
    assert(!deimos::legacy_rule_tracking_query(world, kNone, {0.0f, 0.0f}, 0));
    assert(deimos::legacy_rule_active_count(world, kFoo) == 2);
    assert(deimos::legacy_rule_active_count(world, kNone) == 0);
    assert(deimos::legacy_destroyable_entities_active(world, kAir));
    assert(!deimos::legacy_destroyable_entities_active(world, kGround));

    world.members()[0].lifecycle = deimos::EntityLifecycle::destroyed;
    assert(deimos::legacy_rule_active_count(world, kFoo) == 1);
    assert(!deimos::legacy_rule_tracking_query(world, kFoo, {0.0f, 0.0f}, 0));

    deimos::PlayerWorld players;
    players.slots()[0] = {4, 3.0f, 4.0f, 0};

    deimos::CompiledStateRule rule;
    rule.unit_id = kFoo;
    rule.range = 6;
    deimos::UnitRuleWorldContext context;
    context.entities = &world;
    context.players = &players;
    context.subject_position = {0.0f, 0.0f};
    context.visibility = context.required_visibility = 100.0f;
    context.tint = context.required_tint = 0.0f;
    context.scale = context.required_scale = 1.0f;

    const auto facts = deimos::build_unit_rule_world_facts(rule, context);
    assert(facts.players_active);
    assert(facts.entity_within_player_range);
    assert(!facts.active); // surviving foo is at x=100, outside range 6
    assert(facts.matching_unit_active_count == 1);
    assert(facts.visibility == facts.required_visibility);

    // Canonical template/default tracking slots use Unit ID 'none'. They must
    // remain inert rather than inheriting the subject entity's target flag.
    rule.unit_id = kNone;
    rule.range = 0;
    const auto sentinel = deimos::build_unit_rule_world_facts(rule, context);
    assert(!sentinel.tracking_player);
    assert(!sentinel.active);
    assert(sentinel.matching_unit_active_count == 0);

    return 0;
}
