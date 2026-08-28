#include "deimos/collision_runtime.hpp"
#include "deimos/destruction_runtime.hpp"
#include "deimos/entity_runtime.hpp"
#include "deimos/terrain_runtime.hpp"
#include "deimos/unit_behavior.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>

namespace {

deimos::FourCC id(const char (&text)[5]) {
    return deimos::FourCC{{text[0], text[1], text[2], text[3]}};
}

deimos::DefinitionField f_bool(const char* key, bool value) {
    return {key, value, value ? "TRUE" : "FALSE", 0};
}
deimos::DefinitionField f_int(const char* key, int value) {
    return {key, value, std::to_string(value), 0};
}
deimos::DefinitionField f_float(const char* key, float value) {
    return {key, value, std::to_string(value), 0};
}
deimos::DefinitionField f_string(const char* key, const char* value) {
    return {key, std::string(value), value, 0};
}
deimos::DefinitionField f_id(const char* key, deimos::FourCC value) {
    return {key, value, value.str(), 0};
}

void add_state_defaults(
    deimos::UnitStateDefinition& state,
    bool shield_depletion_state) {
    state.fields.add(f_bool("stateUseThisStateOnShieldDepletion_BOOL", shield_depletion_state));
    state.fields.add(f_bool("stateCollides_BOOL", true));
    state.fields.add(f_bool("stateInvulnerable_ShieldsDoNotDepleteOnCollision_BOOL", false));
    state.fields.add(f_bool("stateDoNotGlowOnCollision_BOOL", false));
    state.fields.add(f_float("stateOnRange_FLOAT", 0.0f));
    state.fields.add(f_string("stateOnRangeChangeTo_STR", ""));
    state.fields.add(f_string("stateOnHitChangeTo_STR", ""));
    state.fields.add(f_int("stateOnHitChangeStateDelay_INT", 0));
    state.fields.add(f_int("stateOnTimerMin_INT", 0));
    state.fields.add(f_int("stateOnTimerMax_INT", 0));
    state.fields.add(f_string("stateOnTimerChangeTo_STR", ""));
    state.fields.add(f_int("stateOnCounter_INT", 0));
    state.fields.add(f_string("stateOnCounterChangeTo_STR", ""));
    state.fields.add(f_id("collision_Spawn_ID", id("none")));
    state.fields.add(f_bool("collision_RepeatSpawns_BOOL", false));
    state.fields.add(f_int("collision_SpawnDelay_INT", 0));
}

deimos::UnitDefinition shield_transition_unit() {
    deimos::UnitDefinition unit;
    unit.name = "Shield transition";
    unit.family_name = "Test";
    unit.core_fields.add(f_bool("isGroundBased_BOOL", false));
    unit.core_fields.add(f_float("damage_FLOAT", 1.0f));
    unit.core_fields.add(f_float("shields_BaseAmount_FLOAT", 5.0f));
    unit.core_fields.add(f_float("shields_LevelIncrement_FLOAT", 0.0f));
    unit.core_fields.add(f_float("shields_MaxAmount_FLOAT", 5.0f));
    unit.core_fields.add(f_int("score_INT", 77));

    deimos::UnitStateDefinition normal;
    normal.name = "Normal";
    add_state_defaults(normal, false);
    unit.states.push_back(std::move(normal));

    deimos::UnitStateDefinition depleted;
    depleted.name = "Depleted";
    add_state_defaults(depleted, true);
    unit.states.push_back(std::move(depleted));
    return unit;
}

} // namespace

int main() {
    // PPC 0x35F88..0x35FA0 derives live +0x19 from UnitDef +0x08 == 'air '.
    // Main tick 0x344F8 therefore prevents air-domain members from ever
    // reaching the ground-obstacle Rect query even if +0x128 is set.
    deimos::LegacyGroundObstacleRects obstacles;
    obstacles.add({5, 5, 15, 15});

    deimos::EntityRuntime obstacle_entity;
    obstacle_entity.x = 10.0f;
    obstacle_entity.y = 10.0f;
    obstacle_entity.collision_half_width = 2;
    obstacle_entity.collision_half_height = 2;
    obstacle_entity.behavior.collides_with_ground_obstacles = true;
    obstacle_entity.behavior.collision_domain = id("air ");
    obstacle_entity.velocity_x = 3.0f;
    obstacle_entity.velocity_y = -2.0f;

    assert(!deimos::legacy_collides_with_ground_obstacle(obstacle_entity, obstacles));
    assert(!deimos::apply_legacy_ground_obstacle_stop(obstacle_entity, obstacles));
    assert(obstacle_entity.velocity_x == 3.0f && obstacle_entity.velocity_y == -2.0f);
    assert(!obstacle_entity.stationary);

    obstacle_entity.behavior.collision_domain = id("grnd");
    assert(deimos::legacy_collides_with_ground_obstacle(obstacle_entity, obstacles));
    assert(deimos::apply_legacy_ground_obstacle_stop(obstacle_entity, obstacles));
    assert(obstacle_entity.velocity_x == 0.0f && obstacle_entity.velocity_y == 0.0f);
    assert(obstacle_entity.stationary);

    // Parser 0x41698..0x416A8 stores
    // stateUseThisStateOnShieldDepletion_BOOL at state +0x356. Constructor
    // 0x35DAC..0x35DF0 scans those bytes into live +0xCD. On zero shields,
    // 0x14F10 awards score and routes +0xCD entities to 0x17E70, which enters
    // the first marked state instead of calling ordinary destruction 0x16300.
    auto unit = shield_transition_unit();
    const auto compiled = deimos::compile_unit_behavior(unit);
    assert(compiled.has_shield_depletion_state);
    assert(!compiled.states[0].use_on_shield_depletion);
    assert(compiled.states[1].use_on_shield_depletion);

    deimos::EntityRuntime target;
    target.handle = 1;
    target.serial = 1;
    target.unit_id = id("shdp");
    deimos::LegacyRandom random(1234);
    deimos::initialize_entity_state_machine(target, unit, 0, random);
    target.shields = 5.0f;

    deimos::LegacyRemovalContext removal_context;
    deimos::LegacyRemovalTrace removal_trace;
    const auto result = deimos::apply_collision_damage(
        target, unit, 20.0f, 4, 2, random, 0,
        &removal_context, &removal_trace);

    assert(result.applied);
    assert(result.shields_before == 5.0f);
    assert(result.shields_after == 0.0f);
    assert(result.score_award == 77);
    assert(result.shield_depletion_state_entered);
    assert(result.shield_depletion_state_index == std::optional<std::size_t>{1});
    assert(!result.entity_destroyed);
    assert(target.lifecycle == deimos::EntityLifecycle::active);
    assert(target.state.current_state == 1);
    assert(removal_trace.consequences.empty());

    std::cout << "core edge runtime tests passed\n";
    return 0;
}
