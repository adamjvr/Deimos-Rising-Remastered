#include "deimos/entity_world.hpp"
#include "deimos/unit_behavior.hpp"

#include <cassert>
#include <cmath>
#include <string>

namespace {

deimos::DefinitionField b(const char* key, bool value) {
    return {key, value, value ? "TRUE" : "FALSE", 1};
}

deimos::DefinitionField i(const char* key, int value) {
    return {key, value, std::to_string(value), 1};
}

deimos::DefinitionField f(const char* key, float value) {
    return {key, value, std::to_string(value), 1};
}

deimos::DefinitionField s(const char* key, const char* value) {
    return {key, std::string(value), value, 1};
}

deimos::DefinitionField idf(const char* key, const char* value) {
    deimos::FourCC id{};
    for (int n = 0; n < 4; ++n) id.bytes[static_cast<std::size_t>(n)] = value[n];
    return {key, id, value, 1};
}

deimos::UnitStateDefinition state(
    const char* name, float range = 0.0f, const char* range_action = "No State") {
    deimos::UnitStateDefinition out;
    out.name = name;
    out.fields.add(f("stateOnRange_FLOAT", range));
    out.fields.add(s("stateOnRangeChangeTo_STR", range_action));
    out.fields.add(s("stateOnHitChangeTo_STR", "No State"));
    out.fields.add(i("stateOnHitChangeStateDelay_INT", 0));
    out.fields.add(i("stateOnTimerMin_INT", 999));
    out.fields.add(i("stateOnTimerMax_INT", 999));
    out.fields.add(s("stateOnTimerChangeTo_STR", "No State"));
    out.fields.add(i("stateOnCounter_INT", 0));
    out.fields.add(s("stateOnCounterChangeTo_STR", "No State"));
    return out;
}

deimos::UnitDefinition unit_with(deimos::UnitStateDefinition st) {
    deimos::UnitDefinition unit;
    unit.name = "motion";
    unit.states.push_back(std::move(st));
    return unit;
}

void set_bool(deimos::UnitStateDefinition& st, const char* key, bool value) {
    st.fields.add(b(key, value));
}

void set_float(deimos::UnitStateDefinition& st, const char* key, float value) {
    st.fields.add(f(key, value));
}

bool near(float a, float b, float eps = 1.0e-4f) {
    return std::fabs(a - b) <= eps;
}

void prepare_live(deimos::EntityRuntime& live, const deimos::UnitDefinition& unit) {
    live.lifecycle = deimos::EntityLifecycle::active;
    live.behavior = deimos::compile_unit_behavior(unit);
    live.spawn_runtime_by_state.resize(unit.states.size());
    live.state.current_state = 0;
}

} // namespace

int main() {
    // Two-slot player query: only status==4 participates and strict '<' keeps
    // an exact tie with the first active slot.
    deimos::PlayerWorld players;
    players.slots()[0] = {4, -10.0f, 0.0f, 7};
    players.slots()[1] = {4, 10.0f, 0.0f, 9};
    assert(players.any_active_player());
    const auto tie = players.closest_active_player(0.0f, 0.0f);
    assert(tie && tie->slot == 0 && tie->player_index == 7);
    assert(near(tie->distance, 10.0f));
    players.slots()[0].status = 3;
    const auto only_second = players.closest_active_player(0.0f, 0.0f);
    assert(only_second && only_second->slot == 1 && only_second->player_index == 9);

    // Hunt: two RNG draws establish the envelope; clamp/reverse occurs before
    // applying the per-axis velocity delta.
    auto hunt_state = state("hunt");
    set_bool(hunt_state, "stateHunts_BOOL", true);
    set_float(hunt_state, "stateHoldMaxSpeed_FLOAT", 10.0f);
    auto hunt_unit = unit_with(std::move(hunt_state));
    deimos::EntityRuntime hunt;
    prepare_live(hunt, hunt_unit);
    hunt.velocity_x = 20.0f;
    hunt.velocity_y = -20.0f;
    hunt.velocity_delta_x = 1.0f;
    hunt.velocity_delta_y = -2.0f;
    deimos::LegacyRandom hunt_rng(1);
    deimos::advance_entity_hunt_motion(hunt, hunt_unit, hunt_rng);
    assert(near(hunt.velocity_x, 6.59f));
    assert(near(hunt.velocity_y, -5.59f));
    assert(near(hunt.target_velocity_x, hunt.velocity_x));
    assert(near(hunt.target_velocity_y, hunt.velocity_y));
    assert(hunt.velocity_delta_x == -1.0f && hunt.velocity_delta_y == 2.0f);
    assert(hunt_rng.seed() == 2524885223u);

    // Hold uses a deliberately negated normalized target displacement.
    auto hold_state = state("hold");
    set_bool(hold_state, "stateHoldPositionToTarget_BOOL", true);
    set_float(hold_state, "stateHoldMaxSpeed_FLOAT", 10.0f);
    set_float(hold_state, "stateHoldDelta_FLOAT", 2.0f);
    auto hold_unit = unit_with(std::move(hold_state));
    deimos::EntityRuntime hold;
    prepare_live(hold, hold_unit);
    hold.has_active_target = true;
    hold.target_player_x = 3.0f;
    hold.target_player_y = 4.0f;
    hold.target_player_distance = 5.0f;
    deimos::advance_entity_hold_motion(hold, hold_unit);
    assert(near(hold.target_velocity_x, -6.0f));
    assert(near(hold.target_velocity_y, -8.0f));
    assert(near(hold.velocity_delta_x, 1.2f));
    assert(near(hold.velocity_delta_y, 1.6f));

    // Cyclic accelerates by axis according to which side of the target the
    // member currently occupies and clamps immediately to stateMaxSpeed.
    auto cyclic_state = state("cyclic");
    set_bool(cyclic_state, "stateCyclicMotion_BOOL", true);
    set_float(cyclic_state, "stateMaxSpeed_FLOAT", 2.0f);
    set_float(cyclic_state, "stateDelta_FLOAT", 0.5f);
    auto cyclic_unit = unit_with(std::move(cyclic_state));
    deimos::EntityRuntime cyclic;
    prepare_live(cyclic, cyclic_unit);
    cyclic.has_active_target = true;
    cyclic.x = -1.0f; cyclic.y = 1.0f;
    cyclic.target_player_x = 0.0f; cyclic.target_player_y = 0.0f;
    deimos::advance_entity_cyclic_motion(cyclic, cyclic_unit);
    assert(near(cyclic.velocity_x, 0.5f) && near(cyclic.velocity_y, -0.5f));
    assert(near(cyclic.velocity_delta_x, 0.5f) && near(cyclic.velocity_delta_y, -0.5f));

    // PPC 0x16CC0 uses the independent Flee speed/delta pair and accelerates
    // toward the destination installed in live +0x11C/+0x120. It does not
    // require an active-player target while +0xCC is set.
    auto flee_state = state("flee");
    set_float(flee_state, "stateFleeSpeed_FLOAT", 2.0f);
    set_float(flee_state, "stateFleeDelta_FLOAT", 0.75f);
    auto flee_unit = unit_with(std::move(flee_state));
    deimos::EntityRuntime flee;
    prepare_live(flee, flee_unit);
    flee.fleeing = true;
    flee.has_active_target = false;
    flee.x = -1.0f; flee.y = 1.0f;
    flee.target_player_x = 0.0f; flee.target_player_y = 0.0f;
    deimos::advance_entity_flee_motion(flee, flee_unit);
    assert(near(flee.velocity_x, 0.75f) && near(flee.velocity_y, -0.75f));

    // PPC 0x17510 canonical target modes and RNG order. nora draws X in the
    // visible 0..416 span, then stores the authored north boundary. cega is
    // deterministic and therefore consumes no RNG.
    deimos::EntityRuntime flee_target;
    deimos::LegacyRandom flee_target_rng(1);
    assert(deimos::initialize_entity_flee_target(
        flee_target, deimos::FourCC{{'n','o','r','a'}}, flee_target_rng));
    assert(flee_target.fleeing);
    assert(near(flee_target.target_player_x,
        deimos::choose_legacy_float(0.0f, 416.0f, 16838u)));
    assert(near(flee_target.target_player_y, -1000.0f));
    assert(flee_target_rng.seed() == 1103527590u);
    const auto before_centre = flee_target_rng.seed();
    assert(deimos::initialize_entity_flee_target(
        flee_target, deimos::FourCC{{'c','e','g','a'}}, flee_target_rng));
    assert(near(flee_target.target_player_x, 208.0f));
    assert(near(flee_target.target_player_y, 240.0f));
    assert(flee_target_rng.seed() == before_centre);

    // 0x161C0 derives spawn rotation from the live sprite frame. Multi-
    // direction states divide by FramesPerDirection; the one-direction path
    // instead treats sprite frame as the rotational index across that span.
    deimos::EntityRuntime visual_heading;
    visual_heading.behavior.states.resize(1);
    visual_heading.state.current_state = 0;
    visual_heading.behavior.states[0].number_of_directions = 24;
    visual_heading.behavior.states[0].frames_per_direction = 2;
    visual_heading.sprite_frame = 14; // direction 7 * 15 degrees
    assert(deimos::legacy_current_entity_visual_heading(visual_heading) == 105);
    visual_heading.behavior.states[0].number_of_directions = 1;
    visual_heading.behavior.states[0].frames_per_direction = 36;
    visual_heading.sprite_frame = 5;
    assert(deimos::legacy_current_entity_visual_heading(visual_heading) == 50);

    // Ordinary convergence is component-wise and stationary zeros the complete
    // motion block.
    deimos::EntityRuntime converge;
    prepare_live(converge, cyclic_unit);
    converge.velocity_x = 0.0f; converge.velocity_y = 4.0f;
    converge.target_velocity_x = 2.0f; converge.target_velocity_y = 2.0f;
    converge.velocity_delta_x = 0.5f; converge.velocity_delta_y = -0.25f;
    deimos::converge_entity_velocity(converge, cyclic_unit);
    assert(near(converge.velocity_x, 0.5f));
    assert(near(converge.velocity_y, 3.75f));
    converge.stationary = true;
    deimos::converge_entity_velocity(converge, cyclic_unit);
    assert(converge.velocity_x == 0.0f && converge.velocity_y == 0.0f);
    assert(converge.target_velocity_x == 0.0f && converge.target_velocity_y == 0.0f);
    assert(converge.velocity_delta_x == 0.0f && converge.velocity_delta_y == 0.0f);

    // Player-aware tick updates target fields and feeds the just-measured range
    // into the existing strict range transition in the same tick.
    auto range_a = state("A", 20.0f, "B");
    auto range_b = state("B");
    deimos::UnitDefinition range_unit;
    range_unit.name = "range";
    range_unit.states.push_back(std::move(range_a));
    range_unit.states.push_back(std::move(range_b));
    deimos::EntityRuntime range_live;
    prepare_live(range_live, range_unit);
    range_live.spawn_runtime_by_state.resize(2);
    deimos::EntityWorld world;
    deimos::LegacyRandom range_rng(1);
    deimos::PlayerWorld range_players;
    range_players.slots()[0] = {4, 10.0f, 0.0f, 3};
    deimos::EntityTickContext tick;
    tick.current_tick = 1;
    const deimos::LegacyTrigTables trig;
    const auto range_result = deimos::advance_entity_runtime_with_players(
        world, range_live, range_unit, tick, range_players, range_rng, trig);
    assert(range_result.range_action_processed);
    assert(range_live.state.current_state == 1);
    assert(range_live.has_active_target && range_live.target_player_index == 3);

    // No-player lifecycle actions are distinct; absent either flag, Hunt still
    // consumes its two RNG draws even though no target exists.
    auto delete_state = state("delete");
    set_bool(delete_state, "stateDeleteOnNoActivePlayers_BOOL", true);
    auto delete_unit = unit_with(std::move(delete_state));
    deimos::EntityRuntime deleted;
    prepare_live(deleted, delete_unit);
    deimos::LegacyRandom delete_rng(1);
    deimos::PlayerWorld empty_players;
    (void)deimos::advance_entity_runtime_with_players(
        world, deleted, delete_unit, tick, empty_players, delete_rng, trig);
    assert(deleted.lifecycle == deimos::EntityLifecycle::deleted);
    assert(delete_rng.seed() == 1u);

    auto destruct_state = state("destruct");
    set_bool(destruct_state, "stateDestructOnNoActivePlayers_BOOL", true);
    auto destruct_unit = unit_with(std::move(destruct_state));
    deimos::EntityRuntime destroyed;
    prepare_live(destroyed, destruct_unit);
    deimos::LegacyRandom destruct_rng(1);
    (void)deimos::advance_entity_runtime_with_players(
        world, destroyed, destruct_unit, tick, empty_players, destruct_rng, trig);
    assert(destroyed.lifecycle == deimos::EntityLifecycle::destroyed);

    auto no_target_hunt_state = state("hunt-no-player");
    set_bool(no_target_hunt_state, "stateHunts_BOOL", true);
    set_float(no_target_hunt_state, "stateHoldMaxSpeed_FLOAT", 10.0f);
    auto no_target_hunt_unit = unit_with(std::move(no_target_hunt_state));
    deimos::EntityRuntime no_target_hunt;
    prepare_live(no_target_hunt, no_target_hunt_unit);
    no_target_hunt.velocity_delta_x = 1.0f;
    no_target_hunt.velocity_delta_y = 1.0f;
    deimos::LegacyRandom no_target_rng(1);
    (void)deimos::advance_entity_runtime_with_players(
        world, no_target_hunt, no_target_hunt_unit, tick, empty_players, no_target_rng, trig);
    assert(no_target_hunt.lifecycle == deimos::EntityLifecycle::active);
    assert(!no_target_hunt.has_active_target);
    assert(no_target_rng.seed() == 2524885223u);

    // UnitDef no-player flee flags invoke 0x17510 and return from 0x15280
    // immediately. The first tick installs the destination only; acceleration
    // starts through the pre-existing-flee early path on the following tick.
    auto no_player_flee_state = state("no-player-flee");
    set_float(no_player_flee_state, "stateFleeSpeed_FLOAT", 4.0f);
    set_float(no_player_flee_state, "stateFleeDelta_FLOAT", 0.5f);
    auto no_player_flee_unit = unit_with(std::move(no_player_flee_state));
    no_player_flee_unit.core_fields.add(b("fleesNorthOnNoActivePlayers_BOOL", true));
    no_player_flee_unit.core_fields.add(b("fleesSouthOnNoActivePlayers_BOOL", false));
    deimos::EntityRuntime no_player_flee;
    prepare_live(no_player_flee, no_player_flee_unit);
    no_player_flee.x = 100.0f;
    no_player_flee.y = 100.0f;
    deimos::LegacyRandom no_player_flee_rng(1);
    (void)deimos::advance_entity_runtime_with_players(
        world, no_player_flee, no_player_flee_unit, tick,
        empty_players, no_player_flee_rng, trig);
    assert(no_player_flee.fleeing);
    assert(near(no_player_flee.target_player_y, -1000.0f));
    assert(near(no_player_flee.velocity_x, 0.0f));
    assert(near(no_player_flee.velocity_y, 0.0f));
    assert(no_player_flee_rng.seed() == 1103527590u);
    tick.current_tick = 2;
    (void)deimos::advance_entity_runtime_with_players(
        world, no_player_flee, no_player_flee_unit, tick,
        empty_players, no_player_flee_rng, trig);
    assert(near(no_player_flee.velocity_y, -0.5f));

    // A range transition into an explicit flee state installs the flee target
    // during state entry, but PPC 0x1550C performs ordinary convergence for
    // the rest of that dispatch. Flee acceleration begins on the next tick.
    auto range_flee_a = state("range-A", 20.0f, "range-flee");
    auto range_flee_b = state("range-flee");
    range_flee_b.fields.add(idf("stateFlee_ID", "cega"));
    set_float(range_flee_b, "stateMaxSpeed_FLOAT", 1.0f);
    set_float(range_flee_b, "stateDelta_FLOAT", 0.25f);
    set_float(range_flee_b, "stateFleeSpeed_FLOAT", 5.0f);
    set_float(range_flee_b, "stateFleeDelta_FLOAT", 1.0f);
    deimos::UnitDefinition range_flee_unit;
    range_flee_unit.states.push_back(std::move(range_flee_a));
    range_flee_unit.states.push_back(std::move(range_flee_b));
    deimos::EntityRuntime range_flee;
    prepare_live(range_flee, range_flee_unit);
    range_flee.spawn_runtime_by_state.resize(2);
    deimos::PlayerWorld close_player;
    close_player.slots()[0] = {4, 10.0f, 0.0f, 0};
    deimos::LegacyRandom range_flee_rng(1);
    tick.current_tick = 3;
    const auto range_flee_result = deimos::advance_entity_runtime_with_players(
        world, range_flee, range_flee_unit, tick,
        close_player, range_flee_rng, trig);
    assert(range_flee_result.range_action_processed);
    assert(range_flee.state.current_state == 1);
    assert(range_flee.fleeing);
    assert(near(range_flee.target_player_x, 208.0f));
    assert(near(range_flee.target_player_y, 240.0f));
    assert(near(range_flee.velocity_x, 0.0f));
    assert(near(range_flee.velocity_y, 0.25f));

    return 0;
}
