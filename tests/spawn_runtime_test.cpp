#include "deimos/spawn_runtime.hpp"

#include <cassert>
#include <cstdint>

namespace {

deimos::FourCC id(const char (&text)[5]) {
    deimos::FourCC value;
    for (int i = 0; i < 4; ++i) value.bytes[static_cast<std::size_t>(i)] = text[i];
    return value;
}

} // namespace

int main() {
    deimos::UnitSpawnSet set;
    set.spawn_id = id("test");
    set.rate_min = 10;
    set.rate_max = 12;
    set.num_in_volley_min = 2;
    set.num_in_volley_max = 4;
    set.delay_between_entities_min = 1;
    set.delay_between_entities_max = 3;
    set.repeat_spawns = true;

    // State-entry RNG order: rate -> volley -> delay.
    deimos::LegacyRandom rng(1);
    const auto init = deimos::initialize_spawn_set_runtime(set, 100, rng);
    const int expected_rate = 10 + static_cast<int>(16838u % 3u);
    const int expected_volley = 2 + static_cast<int>(5758u % 3u);
    const int expected_delay = 1 + static_cast<int>(10113u % 3u);
    assert(init.runtime.rate_delay == expected_rate);
    assert(init.runtime.initial_volley_size == expected_volley);
    assert(init.runtime.remaining_in_volley == expected_volley);
    assert(init.runtime.inter_entity_delay == expected_delay);
    assert(init.runtime.rate_anchor_tick == 100u);
    assert(init.runtime.active);

    // A none set is inert and consumes no RNG.
    deimos::UnitSpawnSet none = set;
    none.spawn_id = id("none");
    deimos::LegacyRandom none_rng(1);
    const auto none_init = deimos::initialize_spawn_set_runtime(none, 77, none_rng);
    assert(!none_init.runtime.active);
    assert(none_init.runtime.rate_anchor_tick == 77u);
    assert(none_rng.seed() == 1u);

    // A pending entity decrements its delay; reaching zero emits exactly one
    // spawn and chooses a fresh delay even when that was the final member.
    deimos::SpawnSetRuntime runtime;
    runtime.active = true;
    runtime.rate_delay = 5;
    runtime.rate_anchor_tick = 100;
    runtime.initial_volley_size = 1;
    runtime.remaining_in_volley = 1;
    runtime.inter_entity_delay = 1;
    deimos::LegacyRandom spawn_rng(1);
    const auto spawn = deimos::advance_spawn_set_schedule(set, runtime, 101, {}, spawn_rng);
    assert(spawn.spawn_due);
    assert(runtime.remaining_in_volley == 0);
    assert(runtime.inter_entity_delay == 1 + static_cast<int>(16838u % 3u));

    // Repeat re-arm does not spawn on the same pass.  Its RNG order is
    // delay -> volley -> next rate, unlike state entry.
    runtime.active = true;
    runtime.rate_delay = 5;
    runtime.rate_anchor_tick = 100;
    runtime.remaining_in_volley = 0;
    runtime.initial_volley_size = 1;
    runtime.inter_entity_delay = 0;
    deimos::LegacyRandom repeat_rng(1);
    const auto rearm = deimos::advance_spawn_set_schedule(set, runtime, 105, {}, repeat_rng);
    assert(!rearm.spawn_due);
    assert(runtime.rate_anchor_tick == 105u);
    assert(runtime.inter_entity_delay == 1 + static_cast<int>(16838u % 3u));
    assert(runtime.initial_volley_size == 2 + static_cast<int>(5758u % 3u));
    assert(runtime.remaining_in_volley == runtime.initial_volley_size);
    assert(runtime.rate_delay == 10 + static_cast<int>(10113u % 3u));

    // Non-repeating sets deactivate on the update *after* their last member
    // has reduced remaining_in_volley to zero.
    deimos::UnitSpawnSet one_shot = set;
    one_shot.repeat_spawns = false;
    runtime.active = true;
    runtime.rate_delay = 0;
    runtime.remaining_in_volley = 0;
    const auto deactivated = deimos::advance_spawn_set_schedule(one_shot, runtime, 200, {}, repeat_rng);
    assert(deactivated.deactivated);
    assert(!runtime.active);

    // Fleeing skip preserves every scheduler counter.
    runtime.active = true;
    runtime.rate_delay = 0;
    runtime.rate_anchor_tick = 10;
    runtime.initial_volley_size = 2;
    runtime.remaining_in_volley = 2;
    runtime.inter_entity_delay = 2;
    const auto before = runtime;
    deimos::SpawnScheduleContext fleeing;
    fleeing.parent_is_fleeing = true;
    const auto fleeing_step = deimos::advance_spawn_set_schedule(set, runtime, 20, fleeing, repeat_rng);
    assert(!fleeing_step.spawn_due);
    assert(runtime.rate_delay == before.rate_delay);
    assert(runtime.rate_anchor_tick == before.rate_anchor_tick);
    assert(runtime.remaining_in_volley == before.remaining_in_volley);
    assert(runtime.inter_entity_delay == before.inter_entity_delay);

    // Don'tSpawnOffscreen cancels only a full, unstarted volley.  A partially
    // emitted volley continues offscreen.
    deimos::UnitSpawnSet onscreen_set = set;
    onscreen_set.dont_spawn_offscreen = true;
    runtime.active = true;
    runtime.rate_delay = 0;
    runtime.initial_volley_size = 3;
    runtime.remaining_in_volley = 3;
    runtime.inter_entity_delay = 0;
    deimos::SpawnScheduleContext offscreen;
    offscreen.parent_is_onscreen = false;
    const auto cancelled = deimos::advance_spawn_set_schedule(onscreen_set, runtime, 1, offscreen, repeat_rng);
    assert(cancelled.volley_cancelled_offscreen);
    assert(runtime.remaining_in_volley == 0);

    runtime.remaining_in_volley = 2;
    runtime.initial_volley_size = 3;
    runtime.inter_entity_delay = 0;
    const auto partial = deimos::advance_spawn_set_schedule(onscreen_set, runtime, 2, offscreen, repeat_rng);
    assert(!partial.volley_cancelled_offscreen);
    assert(partial.spawn_due);
    assert(runtime.remaining_in_volley == 1);

    // Rotation-pausing applies only between the first and final member.
    deimos::UnitSpawnSet rotating = set;
    rotating.pause_rotation_while_spawning = true;
    runtime.active = true;
    runtime.initial_volley_size = 3;
    runtime.remaining_in_volley = 3;
    assert(!deimos::spawn_set_is_mid_volley_for_rotation_pause(rotating, runtime));
    runtime.remaining_in_volley = 2;
    assert(deimos::spawn_set_is_mid_volley_for_rotation_pause(rotating, runtime));
    runtime.remaining_in_volley = 0;
    assert(!deimos::spawn_set_is_mid_volley_for_rotation_pause(rotating, runtime));

    // PPC 0x15D8C..0x15DAC: target UnitDef +0x132 is exactly
    // #terrainEffect_BOOL. Terrain effects require a moving parent with its
    // inherited terrain-effects option enabled; ordinary units bypass it.
    deimos::SpawnTargetEligibilityContext eligibility;
    assert(deimos::spawn_target_is_eligible(eligibility));
    eligibility.target_is_terrain_effect = true;
    assert(!deimos::spawn_target_is_eligible(eligibility));
    eligibility.parent_terrain_effects_enabled = true;
    assert(deimos::spawn_target_is_eligible(eligibility));
    eligibility.parent_is_stationary = true;
    assert(!deimos::spawn_target_is_eligible(eligibility));

    // Bridge the parsed Unit Definition tags into the original target gate
    // and portable spawn-request subset.
    deimos::UnitDefinition terrain_target;
    terrain_target.core_fields.add({"terrainEffect_BOOL", true, "TRUE", 1});
    terrain_target.core_fields.add({"adjustInitialLocForOwnerScale_BOOL", false, "FALSE", 2});

    // Spawn-placement geometry from PPC 0x15E18..0x16158.
    const deimos::LegacyTrigTables trig;
    deimos::SpawnPlacementContext placement_context;
    placement_context.parent_x = 100.0f;
    placement_context.parent_y = 200.0f;
    placement_context.parent_heading_degrees = 0;

    deimos::UnitSpawnSet placement_set;
    placement_set.spawn_id = id("test");
    placement_set.x_offset = 10;
    placement_set.y_offset = -5;
    auto placement = deimos::compute_spawn_placement(placement_set, placement_context, trig);
    assert(placement.x == 110.0f);
    assert(placement.y == 195.0f);
    assert(!placement.heading_is_set);

    // Absolute coordinates use a zero base when rotation adjustment is off.
    placement_set.absolute_coordinates = true;
    placement = deimos::compute_spawn_placement(placement_set, placement_context, trig);
    assert(placement.x == 10.0f);
    assert(placement.y == -5.0f);

    // Owner scaling applies only when the target-definition flag observed at
    // +0x12E is set and parent scale differs from exactly 1.0f.
    placement_context.parent_scale = 2.0f;
    placement_context.target_adjusts_initial_location_for_owner_scale = true;
    placement_set.absolute_coordinates = false;
    placement = deimos::compute_spawn_placement(placement_set, placement_context, trig);
    assert(placement.x == 120.0f);
    assert(placement.y == 190.0f);

    // Rotated offsets use the parent as the base and truncate the fused result
    // toward zero.  Heading 90 turns +X into +Y.
    placement_context.parent_scale = 1.0f;
    placement_context.target_adjusts_initial_location_for_owner_scale = false;
    placement_context.parent_heading_degrees = 90;
    placement_set.adjust_offset_for_unit_rotation = true;
    placement_set.absolute_coordinates = false;
    placement_set.x_offset = 10;
    placement_set.y_offset = 0;
    placement = deimos::compute_spawn_placement(placement_set, placement_context, trig);
    assert(placement.x == 100.0f);
    assert(placement.y == 210.0f);

    // SetHeading changes both the child heading and the angle used to rotate
    // offsets.  Original code wraps >359 exactly once.
    placement_context.parent_heading_degrees = 350;
    placement_set.set_heading = true;
    placement_set.heading_degrees = 20;
    placement = deimos::compute_spawn_placement(placement_set, placement_context, trig);
    assert(placement.heading_is_set);
    assert(placement.heading_degrees == 10);
    assert(placement.x >= 109.0f && placement.x <= 110.0f);
    assert(placement.y >= 201.0f && placement.y <= 202.0f);

    // Without rotation adjustment, SetHeading uses the raw configured heading.
    placement_set.adjust_offset_for_unit_rotation = false;
    placement_set.set_heading = true;
    placement_set.heading_degrees = 275;
    placement = deimos::compute_spawn_placement(placement_set, placement_context, trig);
    assert(placement.heading_degrees == 275);

    deimos::SpawnRequestContext request_context;
    request_context.placement = placement_context;
    request_context.parent_terrain_effects_enabled = false;
    // terrainEffect target is rejected until the parent enables terrain effects.
    assert(!deimos::build_spawn_request_seed(placement_set, terrain_target, request_context, trig));
    request_context.parent_terrain_effects_enabled = true;
    const auto request = deimos::build_spawn_request_seed(placement_set, terrain_target, request_context, trig);
    assert(request.has_value());
    assert(request->unit_id == placement_set.spawn_id);
    assert(request->heading_is_set);
    assert(request->heading_degrees == 275);
    assert(request->stationary == placement_set.stationary_option);
    assert(request->terrain_effects_enabled == placement_set.terrain_effects_option);

    return 0;
}
