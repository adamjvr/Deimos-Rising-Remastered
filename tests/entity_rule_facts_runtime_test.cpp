#include "deimos/entity_rule_facts_runtime.hpp"

#include <cassert>

namespace {

deimos::FourCC id(char a, char b, char c, char d) {
    return deimos::FourCC{{a,b,c,d}};
}

deimos::EntityRuntime entity(deimos::FourCC unit, float x, float y) {
    deimos::EntityRuntime e;
    e.unit_id = unit;
    e.x = x;
    e.y = y;
    e.lifecycle = deimos::EntityLifecycle::active;
    return e;
}

} // namespace

int main() {
    deimos::EntityWorld world;
    // Directly exercising the world-fact query is easier with a constructed
    // group registry in production, but tests can use the mutable member view
    // because facts consume exactly that active-member list.
    world.members().push_back(entity(id('f','o','o','1'), 0.0f, 0.0f));
    world.members().push_back(entity(id('f','o','o','1'), 50.0f, 0.0f));
    world.members().push_back(entity(id('b','a','r','1'), 5.0f, 0.0f));
    world.members()[1].has_active_target = true;
    world.members()[2].behavior.can_be_hit_by_player_projectile = true;
    world.members()[2].behavior.collision_domain = id('a','i','r',' ');

    deimos::PlayerWorld players;
    players.slots()[0] = {4, 3.0f, 4.0f, 0};

    deimos::CompiledStateRule rule;
    rule.unit_id = id('f','o','o','1');
    rule.range = 0;
    auto facts = deimos::build_entity_rule_facts(world, players, world.members()[0], rule);
    assert(facts.active);
    assert(facts.tracking_player);
    assert(facts.matching_unit_active_count == 2);
    assert(facts.destroyable_air_entities_active);
    assert(facts.players_active);

    // Spatial Unit-ID queries use strict distance < range while count remains
    // global, matching the separate active_count(rule.unit_id) contract.
    rule.range = 10;
    facts = deimos::build_entity_rule_facts(world, players, world.members()[0], rule);
    assert(facts.active); // self is a matching active entity at distance zero
    assert(!facts.tracking_player); // tracking foo1 is 50px away
    assert(facts.matching_unit_active_count == 2);
    assert(facts.entity_within_player_range); // player distance is exactly 5 < 10

    rule.range = 5;
    facts = deimos::build_entity_rule_facts(world, players, world.members()[0], rule);
    assert(!facts.entity_within_player_range); // strict <, not <=

    // Canonical data has thousands of "Is Tracking Player [none]" slots. The
    // sentinel must never alias an arbitrary entity or they self-delete.
    rule.unit_id = id('n','o','n','e');
    rule.range = 0;
    facts = deimos::build_entity_rule_facts(world, players, world.members()[0], rule);
    assert(!facts.active);
    assert(!facts.tracking_player);
    assert(facts.matching_unit_active_count == 0);

    deimos::EntityRuleVisualFacts visual;
    visual.animation_stopped = true;
    visual.visibility = visual.required_visibility = 42.0f;
    visual.tint = visual.required_tint = 3.0f;
    visual.scale = visual.required_scale = 1.5f;
    facts = deimos::build_entity_rule_facts(world, players, world.members()[0], rule, visual);
    assert(facts.animation_stopped);
    assert(facts.visibility == facts.required_visibility);
    assert(facts.tint == facts.required_tint);
    assert(facts.scale == facts.required_scale);
    return 0;
}
