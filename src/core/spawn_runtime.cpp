#include "deimos/spawn_runtime.hpp"

#include <cstdint>
#include <cmath>

namespace deimos {
namespace {

bool signed_tick_less(std::uint32_t lhs, std::uint32_t rhs) {
    return static_cast<std::int32_t>(lhs) < static_cast<std::int32_t>(rhs);
}

} // namespace

bool is_none_spawn_id(const FourCC& id) {
    return id.str() == "none";
}

SpawnPlacement compute_spawn_placement(
    const UnitSpawnSet& spawn_set,
    const SpawnPlacementContext& context,
    const LegacyTrigTables& trig) {
    SpawnPlacement result;
    result.heading_is_set = spawn_set.set_heading;
    result.heading_degrees = spawn_set.heading_degrees;

    const bool apply_owner_scale =
        context.parent_scale != 1.0f && context.target_adjusts_initial_location_for_owner_scale;
    const float scale = apply_owner_scale ? context.parent_scale : 1.0f;

    if (!spawn_set.adjust_offset_for_unit_rotation) {
        // PPC 0x15E18..0x15F30.  Absolute coordinates leave the zeroed spawn
        // request template as the base; relative coordinates copy parent x/y.
        const float base_x = spawn_set.absolute_coordinates ? 0.0f : context.parent_x;
        const float base_y = spawn_set.absolute_coordinates ? 0.0f : context.parent_y;
        const float offset_x = static_cast<float>(static_cast<float>(spawn_set.x_offset) * scale);
        const float offset_y = static_cast<float>(static_cast<float>(spawn_set.y_offset) * scale);
        result.x = static_cast<float>(base_x + offset_x);
        result.y = static_cast<float>(base_y + offset_y);
        return result;
    }

    // PPC 0x15F7C..0x1613C.  Rotated offsets always use the parent position;
    // the absolute-coordinate branch is bypassed.  Canonical 1.0.6 has no set
    // where both flags are true, but this ordering preserves executable
    // behavior for compatibility data.
    int heading = context.parent_heading_degrees;
    if (spawn_set.set_heading) {
        heading += spawn_set.heading_degrees;
        if (heading > 359) heading -= 360; // exactly one wrap in original code
        result.heading_degrees = heading;
    }

    const float cosine = trig.cosine(heading);
    const float sine = trig.sine(heading);
    const float x_scaled = static_cast<float>(static_cast<float>(spawn_set.x_offset) * scale);
    const float y_scaled = static_cast<float>(static_cast<float>(spawn_set.y_offset) * scale);

    // Preserve the PPC instruction sequence:
    //   ySin = fmuls(y, sin); yCos = fmuls(y, cos)
    //   rotX = fmsubs(x, cos, ySin)
    //   rotY = fmadds(x, sin, yCos)
    // followed by fctiwz (truncate toward zero).
    const float y_sin = static_cast<float>(y_scaled * sine);
    const float y_cos = static_cast<float>(y_scaled * cosine);
    const float rotated_x = std::fma(x_scaled, cosine, -y_sin);
    const float rotated_y = std::fma(x_scaled, sine, y_cos);
    const int integer_x = static_cast<int>(std::trunc(rotated_x));
    const int integer_y = static_cast<int>(std::trunc(rotated_y));

    result.x = static_cast<float>(context.parent_x + static_cast<float>(integer_x));
    result.y = static_cast<float>(context.parent_y + static_cast<float>(integer_y));
    return result;
}


bool spawn_target_is_eligible(const SpawnTargetEligibilityContext& context) {
    // PPC 0x15D8C..0x15DAC.  Non-terrain-effect units bypass the special
    // gate.  Terrain effects are rejected for stationary parents and for
    // parents whose inherited terrain-effects option is disabled.
    if (!context.target_is_terrain_effect) return true;
    if (context.parent_is_stationary) return false;
    return context.parent_terrain_effects_enabled;
}

SpawnTargetProperties spawn_target_properties(const UnitDefinition& target_definition) {
    SpawnTargetProperties result;
    result.terrain_effect = target_definition.core_fields
        .bool_value("terrainEffect_BOOL")
        .value_or(false);
    result.adjust_initial_location_for_owner_scale = target_definition.core_fields
        .bool_value("adjustInitialLocForOwnerScale_BOOL")
        .value_or(false);
    return result;
}

std::optional<SpawnRequestSeed> build_spawn_request_seed(
    const UnitSpawnSet& spawn_set,
    const UnitDefinition& target_definition,
    const SpawnRequestContext& context,
    const LegacyTrigTables& trig) {
    const auto properties = spawn_target_properties(target_definition);
    if (!spawn_target_is_eligible({
            properties.terrain_effect,
            context.parent_is_stationary,
            context.parent_terrain_effects_enabled})) {
        return std::nullopt;
    }

    auto placement_context = context.placement;
    placement_context.target_adjusts_initial_location_for_owner_scale =
        properties.adjust_initial_location_for_owner_scale;
    const auto placement = compute_spawn_placement(spawn_set, placement_context, trig);

    SpawnRequestSeed request;
    request.unit_id = spawn_set.spawn_id;
    request.x = placement.x;
    request.y = placement.y;
    request.heading_is_set = placement.heading_is_set;
    request.heading_degrees = placement.heading_degrees;
    request.player_owner_index = context.parent_player_owner_index;
    request.parent = context.parent_reference;
    // The global request template leaves +0x0C/+0x18 at zero and +0x28 at
    // 1.0 for child spawns. PPC 0x16178..0x16184 copies these two spawn-set
    // bytes into request +0x1C/+0x1D.
    // +0x1C/+0x1D; the entity constructor later inherits them.
    request.stationary = spawn_set.stationary_option;
    request.terrain_effects_enabled = spawn_set.terrain_effects_option;
    return request;
}

SpawnStateEntryResult initialize_spawn_set_runtime(
    const UnitSpawnSet& spawn_set,
    std::uint32_t current_tick,
    LegacyRandom& random) {
    SpawnStateEntryResult result;
    auto& runtime = result.runtime;
    runtime.rate_anchor_tick = current_tick;

    if (is_none_spawn_id(spawn_set.spawn_id)) {
        // PPC 0x17DD4..0x17DE4: none leaves the record inactive and resets the
        // parent rotation-pause field.  The freshly allocated record's other
        // counters are already zero.
        result.rotation_pause_ticks = 0;
        return result;
    }

    // PPC 0x17D5C..0x17DC4.  The order is part of the RNG stream contract.
    runtime.rate_delay = choose_inclusive_integer(spawn_set.rate_min, spawn_set.rate_max, random);
    runtime.initial_volley_size = choose_inclusive_integer(
        spawn_set.num_in_volley_min, spawn_set.num_in_volley_max, random);
    runtime.active = runtime.rate_delay >= 0 && runtime.initial_volley_size > 0;
    runtime.remaining_in_volley = runtime.initial_volley_size;
    runtime.inter_entity_delay = choose_inclusive_integer(
        spawn_set.delay_between_entities_min, spawn_set.delay_between_entities_max, random);

    result.rotation_pause_ticks = spawn_set.time_to_pause_rotation_after_spawning;
    return result;
}

SpawnScheduleStep advance_spawn_set_schedule(
    const UnitSpawnSet& spawn_set,
    SpawnSetRuntime& runtime,
    std::uint32_t current_tick,
    const SpawnScheduleContext& context,
    LegacyRandom& random) {
    SpawnScheduleStep result;

    if (is_none_spawn_id(spawn_set.spawn_id) || !runtime.active || runtime.rate_delay < 0) {
        return result;
    }

    // PPC 0x15C14..0x15C28: fleeing parents simply skip sets that did not opt
    // into spawning while fleeing.  The runtime counters are not modified.
    if (context.parent_is_fleeing && !spawn_set.spawn_if_fleeing) {
        return result;
    }

    // Don'tSpawnOffscreen is tested only while a full, not-yet-started volley
    // is pending.  Once at least one member has spawned, the rest of that
    // volley is allowed to finish even if the parent goes offscreen.
    if (spawn_set.dont_spawn_offscreen &&
        runtime.remaining_in_volley > 0 &&
        runtime.remaining_in_volley >= runtime.initial_volley_size &&
        !context.parent_is_onscreen) {
        runtime.remaining_in_volley = 0;
        result.volley_cancelled_offscreen = true;
        return result;
    }

    if (runtime.remaining_in_volley > 0) {
        if (runtime.inter_entity_delay > 0) {
            --runtime.inter_entity_delay;
        }
        if (runtime.inter_entity_delay > 0) {
            return result;
        }

        // PPC 0x15CA0..0x15CC0.  Even the last entity in a volley chooses the
        // next per-entity delay, consuming RNG if the endpoints differ.
        result.spawn_due = true;
        --runtime.remaining_in_volley;
        runtime.inter_entity_delay = choose_inclusive_integer(
            spawn_set.delay_between_entities_min,
            spawn_set.delay_between_entities_max,
            random);
        return result;
    }

    if (!spawn_set.repeat_spawns) {
        runtime.active = false;
        result.deactivated = true;
        return result;
    }

    const auto target_tick = runtime.rate_anchor_tick + static_cast<std::uint32_t>(runtime.rate_delay);
    if (signed_tick_less(current_tick, target_tick)) {
        return result;
    }

    runtime.rate_anchor_tick = current_tick;

    // PPC 0x15CF8..0x15D38.  Re-arming deliberately consumes RNG in a
    // different order from state entry: delay -> volley size -> next rate.
    runtime.inter_entity_delay = choose_inclusive_integer(
        spawn_set.delay_between_entities_min,
        spawn_set.delay_between_entities_max,
        random);
    runtime.initial_volley_size = choose_inclusive_integer(
        spawn_set.num_in_volley_min,
        spawn_set.num_in_volley_max,
        random);
    runtime.remaining_in_volley = runtime.initial_volley_size;
    runtime.rate_delay = choose_inclusive_integer(spawn_set.rate_min, spawn_set.rate_max, random);

    if (spawn_set.pause_rotation_while_spawning &&
        spawn_set.time_to_pause_rotation_after_spawning > context.current_rotation_pause_ticks) {
        result.rotation_pause_updated = true;
        result.requested_rotation_pause_ticks = spawn_set.time_to_pause_rotation_after_spawning;
    }

    // The original function does not emit the first member on the same pass
    // that re-arms a volley; it returns to the loop and waits for a later
    // entity update.
    return result;
}

bool spawn_set_is_mid_volley_for_rotation_pause(
    const UnitSpawnSet& spawn_set,
    const SpawnSetRuntime& runtime) {
    return runtime.active &&
           spawn_set.pause_rotation_while_spawning &&
           runtime.remaining_in_volley > 0 &&
           runtime.remaining_in_volley < runtime.initial_volley_size;
}

} // namespace deimos
