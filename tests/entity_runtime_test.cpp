#include "deimos/entity_runtime.hpp"

#include <cassert>
#include <cmath>
#include <string>

namespace {

deimos::FourCC id(const char* s) {
    deimos::FourCC out;
    for (int i = 0; i < 4; ++i) out.bytes[static_cast<std::size_t>(i)] = s[i];
    return out;
}

deimos::DefinitionField field_bool(const char* key, bool value) {
    return {key, value, value ? "TRUE" : "FALSE", 1};
}

deimos::DefinitionField field_int(const char* key, int value) {
    return {key, value, std::to_string(value), 1};
}

deimos::DefinitionField field_float(const char* key, float value) {
    return {key, value, std::to_string(value), 1};
}

deimos::DefinitionField field_string(const char* key, const char* value) {
    return {key, std::string(value), value, 1};
}

deimos::UnitStateDefinition basic_state(const char* name, int timer_min = 0, int timer_max = 0) {
    deimos::UnitStateDefinition state;
    state.name = name;
    state.fields.add(field_float("stateOnRange_FLOAT", 0.0f));
    state.fields.add(field_string("stateOnRangeChangeTo_STR", "No State"));
    state.fields.add(field_string("stateOnHitChangeTo_STR", "No State"));
    state.fields.add(field_int("stateOnHitChangeStateDelay_INT", 0));
    state.fields.add(field_int("stateOnTimerMin_INT", timer_min));
    state.fields.add(field_int("stateOnTimerMax_INT", timer_max));
    state.fields.add(field_string("stateOnTimerChangeTo_STR", "No State"));
    state.fields.add(field_int("stateOnCounter_INT", 0));
    state.fields.add(field_string("stateOnCounterChangeTo_STR", "No State"));
    return state;
}

void add_constructor_defaults(deimos::UnitDefinition& unit) {
    auto add_if_missing = [&](deimos::DefinitionField field) {
        if (!unit.core_fields.contains(field.key)) unit.core_fields.add(std::move(field));
    };
    add_if_missing(field_int("numInGroupMin_INT", 1));
    add_if_missing(field_int("numInGroupMax_INT", 1));
    add_if_missing(field_int("appearsPercent_INT", 100));
    add_if_missing(field_int("groupDelayMin_INT", 0));
    add_if_missing(field_int("groupDelayMax_INT", 0));
    add_if_missing(field_bool("canBeSpawnedOnlyWhenPlayersActive_BOOL", false));
    add_if_missing(field_bool("doNotSpawnIfTypeAlreadyExists_BOOL", false));
    add_if_missing(field_bool("deleteExistingEntitiesOfThisTypeOwnedByPlayer_BOOL", false));
    add_if_missing(field_bool("randomiseInitialLoc_BOOL", false));
    add_if_missing(field_bool("initialHeadingSetInEditor_BOOL", false));
    add_if_missing(field_bool("initiallyHuntsClosestPlayer_BOOL", false));
    add_if_missing(field_bool("useOwnerHeading_BOOL", false));
    add_if_missing(field_bool("doBurst_BOOL", false));
    add_if_missing(field_bool("doImplode_BOOL", false));
    add_if_missing(field_float("xOffsetMin_FLOAT", 0.0f));
    add_if_missing(field_float("xOffsetMax_FLOAT", 0.0f));
    add_if_missing(field_float("yOffsetMin_FLOAT", 0.0f));
    add_if_missing(field_float("yOffsetMax_FLOAT", 0.0f));
    add_if_missing(field_float("initialSpeedMin_FLOAT", 0.0f));
    add_if_missing(field_float("initialSpeedMax_FLOAT", 0.0f));
    add_if_missing(field_int("initialHeading_INT", 0));
    add_if_missing(field_int("initialHeadingTolerance_INT", 0));
}

bool near(float a, float b, float eps = 1.0e-5f) {
    return std::fabs(a - b) <= eps;
}

} // namespace

int main() {
    const deimos::LegacyTrigTables trig;

    // Group selection happens before player gating, so a rejected request can
    // and does advance the global RNG stream.
    deimos::UnitDefinition gated;
    gated.core_fields.add(field_int("numInGroupMin_INT", 2));
    gated.core_fields.add(field_int("numInGroupMax_INT", 4));
    gated.core_fields.add(field_bool("canBeSpawnedOnlyWhenPlayersActive_BOOL", true));
    add_constructor_defaults(gated);
    deimos::SpawnRequestSeed gated_request;
    gated_request.unit_id = id("gate");
    deimos::EntityConstructionContext gated_context;
    gated_context.player_gate.qualifying_player_present = false;
    deimos::LegacyRandom gated_rng(1);
    const auto gated_plan = deimos::prepare_entity_group_construction(
        gated, gated_request, gated_context, gated_rng);
    assert(gated_plan.rejection == deimos::EntityConstructionRejection::no_qualifying_player);
    assert(gated_rng.seed() == 1103527590u); // variable group size consumed one draw

    // Group base Y uses fctiwz(request.y) - integer world-origin when +0x0C is set.
    deimos::SpawnRequestSeed origin_request;
    origin_request.unit_id = id("orig");
    origin_request.x = 12.5f;
    origin_request.y = 123.9f;
    origin_request.subtract_world_y_origin = true;
    origin_request.editor_heading_degrees = 77;
    origin_request.stationary = true;
    origin_request.terrain_effects_enabled = true;
    const auto origin_group = deimos::build_entity_group_runtime(origin_request, 3, 9, 100);
    assert(origin_group.serial == 9u);
    assert(origin_group.member_count == 3);
    assert(origin_group.base_position.x == 12.5f);
    assert(origin_group.base_position.y == 23.0f);
    assert(origin_group.editor_heading_degrees == 77);
    assert(origin_group.stationary && origin_group.terrain_effects_enabled);

    // PPC 0x37930: both-variable/non-radial placement consumes exactly one
    // angle draw and ignores minima, using abs(maxX)/abs(maxY).
    deimos::UnitDefinition position_unit;
    position_unit.core_fields.add(field_float("xOffsetMin_FLOAT", -10.0f));
    position_unit.core_fields.add(field_float("xOffsetMax_FLOAT", 10.0f));
    position_unit.core_fields.add(field_float("yOffsetMin_FLOAT", -20.0f));
    position_unit.core_fields.add(field_float("yOffsetMax_FLOAT", 20.0f));
    add_constructor_defaults(position_unit);
    deimos::LegacyRandom position_rng(1);
    const auto pos = deimos::choose_initial_member_position(
        position_unit, deimos::build_entity_group_runtime({}, 1, 0, 0), position_rng, trig);
    assert(pos.rng_draws == 1);
    assert(position_rng.seed() == 1103527590u);
    const int expected_angle = static_cast<int>(16838u % 360u);
    assert(near(pos.position.x, trig.sine(expected_angle) * 10.0f));
    assert(near(pos.position.y, trig.cosine(expected_angle) * 20.0f));

    // Reversed one-axis offsets are sorted before fctiwz/inclusive integer RNG.
    deimos::UnitDefinition reversed_axis;
    reversed_axis.core_fields.add(field_float("yOffsetMin_FLOAT", 100.0f));
    reversed_axis.core_fields.add(field_float("yOffsetMax_FLOAT", 0.0f));
    add_constructor_defaults(reversed_axis);
    deimos::LegacyRandom reversed_rng(1);
    const auto reversed_pos = deimos::choose_initial_member_position(
        reversed_axis, deimos::build_entity_group_runtime({}, 1, 0, 0), reversed_rng, trig);
    assert(reversed_pos.position.x == 0.0f);
    assert(reversed_pos.position.y == static_cast<float>(16838u % 101u));

    // A request/editor heading is jittered in the member constructor before
    // motion. Stationary then skips speed RNG, so this consumes exactly one draw.
    deimos::UnitDefinition stationary_heading;
    stationary_heading.core_fields.add(field_int("initialHeadingTolerance_INT", 20));
    add_constructor_defaults(stationary_heading);
    deimos::LegacyRandom heading_rng(1);
    const int jittered = deimos::choose_initial_member_heading(
        stationary_heading, true, 90, heading_rng);
    assert(jittered == 97); // -10 + (16838 % 21) == +7
    const auto stationary_motion = deimos::choose_initial_member_motion(
        stationary_heading, {}, {}, true, true, jittered, 1.0f, {}, heading_rng, trig);
    assert(stationary_motion.status == deimos::EntityInitialMotionStatus::complete);
    assert(stationary_motion.velocity_x == 0.0f && stationary_motion.velocity_y == 0.0f);
    assert(stationary_motion.heading_degrees == 97);
    assert(heading_rng.seed() == 1103527590u);

    // Default initial-heading tolerance is consumed inside the motion path,
    // after speed selection. Equal speed endpoints consume no RNG.
    deimos::UnitDefinition moving;
    moving.core_fields.add(field_float("initialSpeedMin_FLOAT", 2.0f));
    moving.core_fields.add(field_float("initialSpeedMax_FLOAT", 2.0f));
    moving.core_fields.add(field_int("initialHeading_INT", 180));
    moving.core_fields.add(field_int("initialHeadingTolerance_INT", 20));
    add_constructor_defaults(moving);
    deimos::LegacyRandom moving_rng(1);
    const auto moving_motion = deimos::choose_initial_member_motion(
        moving, {}, {}, false, false, 180, 1.0f, {}, moving_rng, trig);
    assert(moving_motion.heading_degrees == 187);
    assert(near(moving_motion.velocity_x, trig.sine(187) * 2.0f));
    assert(near(moving_motion.velocity_y, trig.cosine(187) * 2.0f));
    assert(moving_rng.seed() == 1103527590u);

    // End-to-end normal group path: two live members share a group serial,
    // receive independent member serials/handles, inherit owner/parent data,
    // enter state zero, then receive cumulative group delays 3 and 6.
    deimos::UnitDefinition unit;
    unit.name = "Synthetic Group";
    unit.core_fields.add(field_int("numInGroupMin_INT", 2));
    unit.core_fields.add(field_int("numInGroupMax_INT", 2));
    unit.core_fields.add(field_int("groupDelayMin_INT", 3));
    unit.core_fields.add(field_int("groupDelayMax_INT", 3));
    unit.core_fields.add(field_bool("initialHeadingSetInEditor_BOOL", true));
    unit.core_fields.add(field_float("initialSpeedMin_FLOAT", 2.0f));
    unit.core_fields.add(field_float("initialSpeedMax_FLOAT", 2.0f));
    add_constructor_defaults(unit);
    unit.states.push_back(basic_state("Initial", 5, 5));

    deimos::SpawnRequestSeed request;
    request.unit_id = id("test");
    request.x = 12.0f;
    request.y = 34.0f;
    request.editor_heading_degrees = 90;
    request.player_owner_index = 2;
    request.parent = {41, 77};

    deimos::EntityHeadlessConstructionContext context;
    context.preflight.current_tick = 100;
    deimos::EntityIdentityCounters identities;
    identities.next_group_serial = 10;
    identities.next_member_serial = 20;
    identities.next_member_handle = 1000;
    deimos::LegacyRandom build_rng(1);
    const auto built = deimos::construct_entity_group_headless(
        unit, request, context, identities, build_rng, trig);
    assert(built.constructed());
    assert(built.group && built.group->serial == 10u && built.group->member_count == 2);
    assert(built.members.size() == 2);
    assert(built.first_member_reference.handle == 1000u);
    assert(built.first_member_reference.serial == 20u);
    assert(built.members[0].serial == 20u && built.members[1].serial == 21u);
    assert(built.members[0].group_serial == 10u && built.members[1].group_serial == 10u);
    assert(built.members[0].parent.handle == 41u && built.members[0].parent.serial == 77u);
    assert(built.members[0].player_owner_index == 2);
    assert(built.members[0].state.current_state == 0 && built.members[1].state.current_state == 0);
    assert(built.members[0].state.state_entry_tick == 100u);
    assert(built.members[0].group_delay_ticks == 3);
    assert(built.members[1].group_delay_ticks == 6);
    assert(near(built.members[0].velocity_x, 2.0f));
    assert(std::fabs(built.members[0].velocity_y) < 1.0e-5f);
    // Every range in this synthetic constructor is equal, so no RNG draw.
    assert(build_rng.seed() == 1u);
    assert(identities.next_group_serial == 11u);
    assert(identities.next_member_serial == 22u);
    assert(identities.next_member_handle == 1002u);

    // Counter-triggered state changes still happen immediately inside member construction.
    deimos::UnitDefinition counter_unit;
    add_constructor_defaults(counter_unit);
    counter_unit.name = "Counter";
    auto first = basic_state("A");
    first.fields = {};
    first.fields.add(field_float("stateOnRange_FLOAT", 0.0f));
    first.fields.add(field_string("stateOnRangeChangeTo_STR", "No State"));
    first.fields.add(field_string("stateOnHitChangeTo_STR", "No State"));
    first.fields.add(field_int("stateOnHitChangeStateDelay_INT", 0));
    first.fields.add(field_int("stateOnTimerMin_INT", 0));
    first.fields.add(field_int("stateOnTimerMax_INT", 0));
    first.fields.add(field_string("stateOnTimerChangeTo_STR", "No State"));
    first.fields.add(field_int("stateOnCounter_INT", 1));
    first.fields.add(field_string("stateOnCounterChangeTo_STR", "B"));
    counter_unit.states.push_back(std::move(first));
    counter_unit.states.push_back(basic_state("B"));
    deimos::SpawnRequestSeed counter_request;
    counter_request.unit_id = id("coun");
    deimos::EntityIdentityCounters counter_ids;
    deimos::LegacyRandom counter_rng(1);
    const auto counter_built = deimos::construct_entity_group_headless(
        counter_unit, counter_request, {}, counter_ids, counter_rng, trig);
    assert(counter_built.constructed() && counter_built.members.size() == 1);
    assert(counter_built.members[0].state.current_state == 1);
    assert(counter_built.members[0].state.state_entry_counts[0] == 0);
    assert(counter_built.members[0].state.state_entry_counts[1] == 1);

    // Existing tick ordering remains intact on a constructed live member.
    deimos::UnitDefinition tick_unit;
    add_constructor_defaults(tick_unit);
    auto t0 = basic_state("T0", 1, 1);
    t0.fields = {};
    t0.fields.add(field_float("stateOnRange_FLOAT", 0.0f));
    t0.fields.add(field_string("stateOnRangeChangeTo_STR", "No State"));
    t0.fields.add(field_string("stateOnHitChangeTo_STR", "No State"));
    t0.fields.add(field_int("stateOnHitChangeStateDelay_INT", 0));
    t0.fields.add(field_int("stateOnTimerMin_INT", 1));
    t0.fields.add(field_int("stateOnTimerMax_INT", 1));
    t0.fields.add(field_string("stateOnTimerChangeTo_STR", "T1"));
    t0.fields.add(field_int("stateOnCounter_INT", 0));
    t0.fields.add(field_string("stateOnCounterChangeTo_STR", "No State"));
    tick_unit.states.push_back(std::move(t0));
    auto t1 = basic_state("T1");
    deimos::UnitStateRule active_rule;
    active_rule.name = "active";
    active_rule.unit_id = id("none");
    active_rule.condition = "Is Active";
    active_rule.action = "T2";
    t1.rules.push_back(active_rule);
    tick_unit.states.push_back(std::move(t1));
    auto t2 = basic_state("T2");
    t2.fields = {};
    t2.fields.add(field_float("stateOnRange_FLOAT", 50.0f));
    t2.fields.add(field_string("stateOnRangeChangeTo_STR", "T3"));
    t2.fields.add(field_string("stateOnHitChangeTo_STR", "No State"));
    t2.fields.add(field_int("stateOnHitChangeStateDelay_INT", 0));
    t2.fields.add(field_int("stateOnTimerMin_INT", 0));
    t2.fields.add(field_int("stateOnTimerMax_INT", 0));
    t2.fields.add(field_string("stateOnTimerChangeTo_STR", "No State"));
    t2.fields.add(field_int("stateOnCounter_INT", 0));
    t2.fields.add(field_string("stateOnCounterChangeTo_STR", "No State"));
    tick_unit.states.push_back(std::move(t2));
    auto t3 = basic_state("T3");
    deimos::UnitSpawnSet immediate;
    immediate.name = "immediate";
    immediate.spawn_id = id("chil");
    immediate.rate_min = 0;
    immediate.rate_max = 0;
    immediate.num_in_volley_min = 1;
    immediate.num_in_volley_max = 1;
    immediate.delay_between_entities_min = 0;
    immediate.delay_between_entities_max = 0;
    t3.spawn_sets.push_back(immediate);
    tick_unit.states.push_back(std::move(t3));

    deimos::EntityIdentityCounters tick_ids;
    deimos::LegacyRandom tick_rng(1);
    auto tick_built = deimos::construct_entity_group_headless(
        tick_unit, deimos::SpawnRequestSeed{id("tick")},
        deimos::EntityHeadlessConstructionContext{deimos::EntityConstructionContext{10}},
        tick_ids, tick_rng, trig);
    assert(tick_built.constructed());
    auto& live = tick_built.members[0];
    deimos::EntityTickContext tick_context;
    tick_context.current_tick = 11;
    tick_context.measured_player_range = 10.0f;
    tick_context.facts_for_rule = [](const deimos::CompiledStateRule&, std::size_t) {
        deimos::UnitRuleFacts facts;
        facts.active = true;
        return facts;
    };
    const auto tick = deimos::advance_entity_runtime(live, tick_unit, tick_context, tick_rng);
    assert(tick.timer_action_processed && tick.rule_matched && tick.range_action_processed);
    assert(live.state.current_state == 3);
    assert(tick.spawns_due.size() == 1);

    return 0;
}
