#include "deimos/entity_world.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <utility>

namespace {

deimos::FourCC id(const char (&text)[5]) {
    return deimos::FourCC{{text[0], text[1], text[2], text[3]}};
}

deimos::DefinitionField field_bool(const char* key, bool value) {
    return {key, value, value ? "TRUE" : "FALSE", 0};
}

deimos::UnitDefinition owner_unit(const char* mode_key) {
    deimos::UnitDefinition unit;
    unit.name = "Owner mode";
    deimos::UnitStateDefinition state;
    state.name = "Initial";
    state.fields.add(field_bool("stateLockToOwnerLoc_BOOL", false));
    state.fields.add(field_bool("stateLinkToOwnerLoc_BOOL", false));
    state.fields.add(field_bool("stateOrbitOwner_BOOL", false));
    if (mode_key) {
        // add() preserves duplicate fields; find() returns the first, so create
        // a fresh state containing only the selected true key instead.
        state.fields = {};
        state.fields.add(field_bool("stateLockToOwnerLoc_BOOL", std::string_view(mode_key) == "stateLockToOwnerLoc_BOOL"));
        state.fields.add(field_bool("stateLinkToOwnerLoc_BOOL", std::string_view(mode_key) == "stateLinkToOwnerLoc_BOOL"));
        state.fields.add(field_bool("stateOrbitOwner_BOOL", std::string_view(mode_key) == "stateOrbitOwner_BOOL"));
    }
    unit.states.push_back(std::move(state));
    return unit;
}

deimos::EntityGroupBuildResult one_member(
    deimos::EntityHandle handle,
    std::uint32_t serial,
    deimos::FourCC unit_id,
    float x,
    float y,
    std::int8_t owner = -1) {
    deimos::EntityGroupBuildResult result;
    result.status = deimos::EntityGroupBuildStatus::complete;
    result.group = deimos::EntityGroupRuntime{};
    result.group->serial = serial + 1000;
    result.group->unit_id = unit_id;
    result.group->member_count = 1;
    deimos::EntityRuntime member;
    member.handle = handle;
    member.serial = serial;
    member.group_serial = result.group->serial;
    member.unit_id = unit_id;
    member.x = x;
    member.y = y;
    member.player_owner_index = owner;
    result.members.push_back(std::move(member));
    return result;
}

bool near(float a, float b, float eps = 1.0e-4f) {
    return std::fabs(a - b) <= eps;
}

} // namespace

int main() {
    deimos::EntityWorld world;
    world.register_group(one_member(10, 100, id("pare"), 10.0f, 20.0f));
    world.register_group(one_member(20, 200, id("chil"), 15.0f, 25.0f, 2));
    world.register_group(one_member(30, 300, id("chil"), 30.0f, 40.0f, 3));

    assert(world.active_member_count() == 3);
    assert(world.has_active_unit(id("pare")));
    assert(world.has_active_unit(id("chil")));

    // Safe reference = handle + current serial + active lifecycle.
    assert(world.resolve_reference({10, 100}) != nullptr);
    assert(world.resolve_reference({10, 99}) == nullptr);
    world.find_member(10)->lifecycle = deimos::EntityLifecycle::deleted;
    assert(world.resolve_reference({10, 100}) == nullptr);
    world.find_member(10)->lifecycle = deimos::EntityLifecycle::active;

    // Delete-existing-owned-type query is owner-selective.
    assert(world.mark_owned_unit_deleted(id("chil"), 2) == 1);
    assert(world.active_member_count() == 2);
    assert(world.find_member(20)->lifecycle == deimos::EntityLifecycle::deleted);
    assert(world.find_member(30)->lifecycle == deimos::EntityLifecycle::active);
    world.find_member(20)->lifecycle = deimos::EntityLifecycle::active;

    // Parent safe reference wins over a valid player owner position.
    auto* child = world.find_member(20);
    child->parent = {10, 100};
    auto player = [](std::int8_t index) -> std::optional<deimos::EntityPoint> {
        if (index == 2) return deimos::EntityPoint{100.0f, 200.0f};
        return std::nullopt;
    };
    auto owner = deimos::resolve_entity_owner_position(world, *child, player);
    assert(owner && owner->x == 10.0f && owner->y == 20.0f);

    // Invalid parent serial falls through to signed player-owner position.
    child->parent.serial = 999;
    owner = deimos::resolve_entity_owner_position(world, *child, player);
    assert(owner && owner->x == 100.0f && owner->y == 200.0f);
    child->parent = {10, 100};

    const deimos::LegacyTrigTables trig;

    // Lock: capture the member-to-owner offset at state entry and thereafter
    // pin the member to current owner + that fixed offset.
    auto lock = owner_unit("stateLockToOwnerLoc_BOOL");
    child->state.current_state = 0;
    child->x = 15.0f;
    child->y = 25.0f;
    child->owner_location_initialized_state.reset();
    assert(deimos::current_owner_location_mode(lock, *child) ==
           deimos::EntityOwnerLocationMode::lock_to_owner_location);
    assert(deimos::initialize_entity_owner_location(*child, lock, deimos::EntityPoint{10.0f, 20.0f}));
    assert(child->owner_offset_x == 5.0f && child->owner_offset_y == 5.0f);
    assert(deimos::advance_entity_owner_location(*child, lock, deimos::EntityPoint{20.0f, 30.0f}, trig));
    assert(child->x == 25.0f && child->y == 35.0f);

    // Link: apply only owner translation since the previous tick, preserving
    // independent child movement performed between owner phases.
    auto link = owner_unit("stateLinkToOwnerLoc_BOOL");
    child->x = 15.0f;
    child->y = 25.0f;
    child->owner_location_initialized_state.reset();
    assert(deimos::initialize_entity_owner_location(*child, link, deimos::EntityPoint{10.0f, 20.0f}));
    child->x = 16.0f; // independent movement before the next Link phase
    child->y = 27.0f;
    assert(deimos::advance_entity_owner_location(*child, link, deimos::EntityPoint{13.0f, 25.0f}, trig));
    assert(child->x == 19.0f && child->y == 32.0f);
    assert(child->previous_owner_x == 13.0f && child->previous_owner_y == 25.0f);

    // Orbit: radius/angle are initialized from owner/member geometry. Live
    // velocity X is truncated and used as the angular increment by 0x37350.
    auto orbit = owner_unit("stateOrbitOwner_BOOL");
    child->x = 0.0f;
    child->y = 10.0f;
    child->velocity_x = 90.0f;
    child->owner_location_initialized_state.reset();
    assert(deimos::initialize_entity_owner_location(*child, orbit, deimos::EntityPoint{0.0f, 0.0f}));
    assert(child->orbit_radius == 10.0f);
    assert(child->orbit_angle_degrees == 180);
    assert(deimos::advance_entity_owner_location(*child, orbit, deimos::EntityPoint{0.0f, 0.0f}, trig));
    assert(near(child->x, -10.0f));
    assert(std::fabs(child->y) < 1.0e-3f);
    assert(child->orbit_angle_degrees == 270);

    // The world convenience path uses the valid parent before player fallback.
    world.find_member(10)->x = 50.0f;
    world.find_member(10)->y = 60.0f;
    child->x = 55.0f;
    child->y = 65.0f;
    child->velocity_x = 0.0f;
    child->owner_location_initialized_state.reset();
    assert(!deimos::advance_entity_owner_location_from_world(world, *child, lock, player, trig));
    assert(child->x == 55.0f && child->y == 65.0f); // first phase initializes + already aligned
    world.find_member(10)->x = 70.0f;
    world.find_member(10)->y = 80.0f;
    assert(deimos::advance_entity_owner_location_from_world(world, *child, lock, player, trig));
    assert(child->x == 75.0f && child->y == 85.0f);


    // World-aware tick: a Lock state entered by timer during this tick has its
    // owner bookkeeping lazily initialized in the original post-range slot,
    // before the spawn scheduler. A later owner move is then followed rigidly.
    deimos::UnitDefinition tick_unit;
    tick_unit.name = "Owner Tick";
    deimos::UnitStateDefinition wait;
    wait.name = "Wait";
    wait.fields.add({"stateOnRange_FLOAT", 0.0f, "0", 0});
    wait.fields.add({"stateOnRangeChangeTo_STR", std::string("No State"), "No State", 0});
    wait.fields.add({"stateOnHitChangeTo_STR", std::string("No State"), "No State", 0});
    wait.fields.add({"stateOnHitChangeStateDelay_INT", 0, "0", 0});
    wait.fields.add({"stateOnTimerMin_INT", 1, "1", 0});
    wait.fields.add({"stateOnTimerMax_INT", 1, "1", 0});
    wait.fields.add({"stateOnTimerChangeTo_STR", std::string("Locked"), "Locked", 0});
    wait.fields.add({"stateOnCounter_INT", 0, "0", 0});
    wait.fields.add({"stateOnCounterChangeTo_STR", std::string("No State"), "No State", 0});
    wait.fields.add(field_bool("stateLockToOwnerLoc_BOOL", false));
    wait.fields.add(field_bool("stateLinkToOwnerLoc_BOOL", false));
    wait.fields.add(field_bool("stateOrbitOwner_BOOL", false));
    deimos::UnitStateDefinition locked = owner_unit("stateLockToOwnerLoc_BOOL").states[0];
    locked.name = "Locked";
    locked.fields.add({"stateOnRange_FLOAT", 0.0f, "0", 0});
    locked.fields.add({"stateOnRangeChangeTo_STR", std::string("No State"), "No State", 0});
    locked.fields.add({"stateOnHitChangeTo_STR", std::string("No State"), "No State", 0});
    locked.fields.add({"stateOnHitChangeStateDelay_INT", 0, "0", 0});
    locked.fields.add({"stateOnTimerMin_INT", 0, "0", 0});
    locked.fields.add({"stateOnTimerMax_INT", 0, "0", 0});
    locked.fields.add({"stateOnTimerChangeTo_STR", std::string("No State"), "No State", 0});
    locked.fields.add({"stateOnCounter_INT", 0, "0", 0});
    locked.fields.add({"stateOnCounterChangeTo_STR", std::string("No State"), "No State", 0});
    tick_unit.states.push_back(std::move(wait));
    tick_unit.states.push_back(std::move(locked));

    deimos::EntityRuntime tick_live;
    tick_live.handle = 40;
    tick_live.serial = 400;
    tick_live.unit_id = id("tick");
    tick_live.parent = {10, 100};
    tick_live.x = 55.0f;
    tick_live.y = 65.0f;
    tick_live.behavior = deimos::compile_unit_behavior(tick_unit);
    tick_live.spawn_runtime_by_state.resize(tick_unit.states.size());
    deimos::LegacyRandom tick_rng(1);
    deimos::enter_entity_state(tick_live, tick_unit, 0, 10, tick_rng);
    auto tick_build = one_member(40, 400, id("tick"), 55.0f, 65.0f);
    tick_build.members[0] = std::move(tick_live);
    world.register_group(std::move(tick_build));
    auto* world_tick_live = world.find_member(40);
    world.find_member(10)->x = 50.0f;
    world.find_member(10)->y = 60.0f;
    deimos::EntityTickContext world_tick_context;
    world_tick_context.current_tick = 11;
    const auto world_tick = deimos::advance_entity_runtime_in_world(
        world, *world_tick_live, tick_unit, world_tick_context, player, tick_rng, trig);
    assert(world_tick.timer_action_processed);
    assert(world_tick_live->state.current_state == 1);
    assert(world_tick_live->owner_location_initialized_state == 1);
    assert(world_tick_live->owner_offset_x == 5.0f && world_tick_live->owner_offset_y == 5.0f);
    world.find_member(10)->x = 80.0f;
    world.find_member(10)->y = 90.0f;
    world_tick_context.current_tick = 12;
    (void)deimos::advance_entity_runtime_in_world(
        world, *world_tick_live, tick_unit, world_tick_context, player, tick_rng, trig);
    assert(world_tick_live->x == 85.0f && world_tick_live->y == 95.0f);

    // Finalized inactive history must not accumulate forever in the portable
    // vector world. Active members and pending-removal members remain intact,
    // while a fully empty group is removed with its finalized member.
    deimos::EntityWorld prune_world;
    auto finalized = one_member(80, 800, id("done"), 0.0f, 0.0f);
    finalized.members[0].lifecycle = deimos::EntityLifecycle::destroyed;
    finalized.members[0].removal_processed = true;
    finalized.group->active_member_count = 0;
    const auto finalized_group_serial = finalized.group->serial;
    prune_world.register_group(std::move(finalized));

    auto pending = one_member(81, 801, id("wait"), 0.0f, 0.0f);
    pending.members[0].lifecycle = deimos::EntityLifecycle::destroyed;
    pending.members[0].removal_processed = false;
    pending.group->active_member_count = 0;
    prune_world.register_group(std::move(pending));

    auto active = one_member(82, 802, id("live"), 0.0f, 0.0f);
    prune_world.register_group(std::move(active));

    const auto pruned = prune_world.prune_finalized_history();
    assert(pruned.members_removed == 1);
    assert(pruned.groups_removed == 1);
    assert(prune_world.find_member(80) == nullptr);
    assert(prune_world.find_group(finalized_group_serial) == nullptr);
    assert(prune_world.find_member(81) != nullptr);
    assert(prune_world.find_member(82) != nullptr);

    return 0;
}
