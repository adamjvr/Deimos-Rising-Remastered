#include "deimos/entity_runtime.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace deimos {
namespace {

int unit_int(const UnitDefinition& unit, const char* key, int fallback) {
    return unit.core_fields.int_value(key).value_or(fallback);
}

bool unit_bool(const UnitDefinition& unit, const char* key, bool fallback = false) {
    return unit.core_fields.bool_value(key).value_or(fallback);
}

float unit_float(const UnitDefinition& unit, const char* key, float fallback) {
    return unit.core_fields.float_value(key).value_or(fallback);
}

bool state_bool(const UnitDefinition& unit, std::size_t state_index, const char* key, bool fallback = false) {
    if (state_index >= unit.states.size()) return fallback;
    return unit.states[state_index].fields.bool_value(key).value_or(fallback);
}

float state_float(const UnitDefinition& unit, std::size_t state_index, const char* key, float fallback = 0.0f) {
    if (state_index >= unit.states.size()) return fallback;
    return unit.states[state_index].fields.float_value(key).value_or(fallback);
}

float f32_mul(float a, float b) {
    return static_cast<float>(a * b);
}

float f32_add(float a, float b) {
    return static_cast<float>(a + b);
}

EntityPoint legacy_heading_vector(int heading, float magnitude, const LegacyTrigTables& trig) {
    return {
        f32_mul(magnitude, trig.sine(heading)),
        f32_mul(magnitude, trig.cosine(heading))
    };
}

EntityPoint legacy_normalize_vector(float x, float y) {
    // PPC 0x42BF0 does not call ordinary hypot().  It truncates X to an
    // integer, computes frsp(float(trunc(X))*X + Y*Y), truncates that sum to
    // an integer, then uses a sqrt table for values <16384 (the table itself
    // is generated at startup as frsp(sqrt(float(i)))).
    const int x_integer = static_cast<int>(std::trunc(x));
    const float y_squared = f32_mul(y, y);
    const float sum = std::fma(static_cast<float>(x_integer), x, y_squared);
    const int squared_integer = static_cast<int>(std::trunc(sum));
    if (squared_integer <= 0) return {};
    const float magnitude = static_cast<float>(std::sqrt(static_cast<float>(squared_integer)));
    if (magnitude == 0.0f) return {};
    return {
        static_cast<float>(x / magnitude),
        static_cast<float>(y / magnitude)
    };
}

int wrap_heading_once_or_zero(int heading) {
    if (heading < 0) heading += 360;
    else if (heading > 359) heading -= 360;
    if (heading < 0 || heading > 359) return 0;
    return heading;
}

float approach_axis(float current, float target, float delta) {
    const float step = std::fabs(delta);
    if (step == 0.0f || current == target) return current;
    if (current < target) return std::min(target, static_cast<float>(current + step));
    return std::max(target, static_cast<float>(current - step));
}

int heading_from_velocity(float x, float y, int fallback) {
    if (x == 0.0f && y == 0.0f) return wrap_heading_once_or_zero(fallback);
    // 0x42CD0 is the floating-vector counterpart of the integer angle helper.
    // Scale before truncation so small sub-unit velocities retain direction; the
    // legacy quadrant convention remains the same as the startup atan table.
    const int sx = static_cast<int>(std::trunc(static_cast<double>(x) * 1024.0));
    const int sy = static_cast<int>(std::trunc(static_cast<double>(y) * 1024.0));
    if (sx == 0 && sy == 0) return wrap_heading_once_or_zero(fallback);
    return legacy_angle_between_integer_points(0, 0, sx, sy);
}

void initialize_state_spawn_runtime(
    EntityRuntime& entity,
    const UnitDefinition& unit,
    std::size_t state_index,
    std::uint32_t current_tick,
    LegacyRandom& random) {
    if (state_index >= unit.states.size()) {
        throw std::out_of_range("entity spawn-runtime state index outside Unit Definition");
    }
    if (entity.spawn_runtime_by_state.size() != unit.states.size()) {
        entity.spawn_runtime_by_state.clear();
        entity.spawn_runtime_by_state.resize(unit.states.size());
    }

    const auto& state = unit.states[state_index];
    auto& destination = entity.spawn_runtime_by_state[state_index].spawn_sets;
    destination.clear();
    destination.reserve(state.spawn_sets.size());

    entity.rotation_pause_ticks = 0;
    for (const auto& spawn_set : state.spawn_sets) {
        auto initialized = initialize_spawn_set_runtime(spawn_set, current_tick, random);
        destination.push_back(initialized.runtime);
        entity.rotation_pause_ticks = initialized.rotation_pause_ticks;
    }
}

void enter_entity_state_impl(
    EntityRuntime& entity,
    const UnitDefinition& unit,
    std::size_t state_index,
    std::uint32_t current_tick,
    LegacyRandom& random,
    std::size_t depth) {
    if (depth > kMaxUnitStates) {
        throw std::runtime_error("counter-triggered state-entry chain exceeded original 20-state bound");
    }
    if (entity.lifecycle != EntityLifecycle::active) return;
    if (state_index >= entity.behavior.states.size() || state_index >= unit.states.size()) {
        throw std::out_of_range("entity state index outside compiled Unit Definition");
    }

    const auto& compiled = entity.behavior.states[state_index];
    const bool first_state_entry = std::all_of(
        entity.state.state_entry_counts.begin(), entity.state.state_entry_counts.end(),
        [](int value) { return value == 0; });
    const std::optional<std::size_t> previous_state_index =
        first_state_entry ? std::nullopt : std::optional<std::size_t>{entity.state.current_state};
    // Original state changes call PPC 0x33600 after the new state becomes
    // current. The world layer owns owner/parent resolution, so invalidate
    // its clean initialization marker here and reinitialize at the recovered
    // owner-location phase later in the same tick.
    entity.owner_location_initialized_state.reset();
    const std::uint32_t timer_random =
        compiled.timer_min == compiled.timer_max ? 0u : random.next15();
    const auto entry = enter_unit_state(
        entity.state, entity.behavior, state_index, current_tick, timer_random);

    initialize_entity_state_motion(entity, unit, previous_state_index);
    initialize_state_spawn_runtime(entity, unit, state_index, current_tick, random);

    if (!entry.counter_threshold_reached) return;

    if (counter_action_resets_entry_count(entry.counter_action)) {
        reset_current_state_entry_count(entity.state);
    }

    switch (entry.counter_action.kind) {
    case StateActionKind::none:
    case StateActionKind::unresolved:
        return;
    case StateActionKind::delete_entity:
        entity.lifecycle = EntityLifecycle::deleted;
        return;
    case StateActionKind::destroy_entity:
        entity.lifecycle = EntityLifecycle::destroyed;
        return;
    case StateActionKind::change_state:
        enter_entity_state_impl(
            entity, unit, entry.counter_action.state_index,
            current_tick, random, depth + 1);
        return;
    }
}

} // namespace

EntityGroupSelection select_entity_group_members(
    const UnitDefinition& unit,
    LegacyRandom& random) {
    EntityGroupSelection result;

    int minimum = unit_int(unit, "numInGroupMin_INT", 1);
    const int maximum = unit_int(unit, "numInGroupMax_INT", minimum);
    if (minimum < 1) minimum = 1;
    if (minimum > maximum) minimum = maximum;

    result.selected_before_appearance =
        minimum == maximum ? minimum : choose_inclusive_integer(minimum, maximum, random);
    result.surviving_members = result.selected_before_appearance;

    const int appears_percent = unit_int(unit, "appearsPercent_INT", 100);
    for (int i = 0; i < result.selected_before_appearance; ++i) {
        if (appears_percent == 100) continue;
        if (appears_percent == 0) {
            --result.surviving_members;
            continue;
        }
        ++result.appearance_rng_rolls;
        const int roll = choose_inclusive_integer(0, 100, random);
        // PPC 0x36A7C..0x36A84 removes the candidate only when roll > P.
        if (roll > appears_percent) --result.surviving_members;
    }
    return result;
}

bool unit_requires_active_players(const UnitDefinition& unit) {
    return unit_bool(unit, "canBeSpawnedOnlyWhenPlayersActive_BOOL");
}

EntityGroupConstructionPlan prepare_entity_group_construction(
    const UnitDefinition& unit,
    const SpawnRequestSeed& request,
    const EntityConstructionContext& context,
    LegacyRandom& random) {
    EntityGroupConstructionPlan plan;

    // PPC 0x332B8 calls 0x369F0 before every following rejection gate.
    plan.group = select_entity_group_members(unit, random);
    if (plan.group.surviving_members <= 0) {
        plan.rejection = EntityConstructionRejection::no_group_members;
        return plan;
    }

    if (unit_requires_active_players(unit)) {
        if (!context.player_gate.global_gate_enabled) {
            plan.rejection = EntityConstructionRejection::player_gate_global_disabled;
            return plan;
        }
        if (!context.player_gate.qualifying_player_present) {
            plan.rejection = EntityConstructionRejection::no_qualifying_player;
            return plan;
        }
        if (context.player_gate.suppression_active) {
            plan.rejection = EntityConstructionRejection::player_gate_suppressed;
            return plan;
        }
    }

    if (unit_bool(unit, "doNotSpawnIfTypeAlreadyExists_BOOL") &&
        context.same_unit_type_already_exists) {
        plan.rejection = EntityConstructionRejection::duplicate_type_exists;
        return plan;
    }

    // PPC adds the surviving member count to the global active-member count
    // and rejects only when the result is greater than 1000.
    if (context.active_live_member_count + plan.group.surviving_members > 1000) {
        plan.rejection = EntityConstructionRejection::live_member_limit;
        return plan;
    }

    if (unit_bool(unit, "deleteExistingEntitiesOfThisTypeOwnedByPlayer_BOOL") &&
        request.player_owner_index != -1) {
        plan.delete_existing_owned_type = true;
        plan.delete_existing_owner_index = request.player_owner_index;
    }

    return plan;
}

EntityGroupRuntime build_entity_group_runtime(
    const SpawnRequestSeed& request,
    int member_count,
    std::uint32_t group_serial,
    int world_y_origin) {
    EntityGroupRuntime group;
    group.serial = group_serial;
    group.unit_id = request.unit_id;
    group.base_position.x = request.x;
    if (request.subtract_world_y_origin) {
        // PPC 0x33530..0x33570: fctiwz request Y, subtract integer world
        // origin, then convert the integer result back to single precision.
        const int y_integer = static_cast<int>(std::trunc(request.y));
        group.base_position.y = static_cast<float>(y_integer - world_y_origin);
    } else {
        group.base_position.y = request.y;
    }
    group.member_count = member_count;
    group.editor_heading_degrees = request.editor_heading_degrees;
    group.stationary = request.stationary;
    group.terrain_effects_enabled = request.terrain_effects_enabled;
    return group;
}

EntityInitialPositionResult choose_initial_member_position(
    const UnitDefinition& unit,
    const EntityGroupRuntime& group,
    LegacyRandom& random,
    const LegacyTrigTables& trig) {
    EntityInitialPositionResult result;

    const float x_min = unit_float(unit, "xOffsetMin_FLOAT", 0.0f);
    const float x_max = unit_float(unit, "xOffsetMax_FLOAT", x_min);
    const float y_min = unit_float(unit, "yOffsetMin_FLOAT", 0.0f);
    const float y_max = unit_float(unit, "yOffsetMax_FLOAT", y_min);

    const bool x_varies = x_min != x_max;
    const bool y_varies = y_min != y_max;

    if (x_varies && y_varies) {
        const int angle = choose_inclusive_integer(0, 359, random);
        ++result.rng_draws;
        const float sine = trig.sine(angle);
        const float cosine = trig.cosine(angle);

        if (unit_bool(unit, "randomiseInitialLoc_BOOL")) {
            const auto before = random.seed();
            const float radius = choose_legacy_float(0.0f, std::fabs(x_max), random);
            if (random.seed() != before) ++result.rng_draws;
            result.position.x = std::fma(sine, radius, group.base_position.x);
            result.position.y = std::fma(cosine, radius, group.base_position.y);
        } else {
            // This strange asymmetry is literal PPC behavior: in the
            // both-variable/non-radial path the minima are ignored and X/Y
            // use abs(max) independently with one shared random angle.
            result.position.x = std::fma(sine, std::fabs(x_max), group.base_position.x);
            result.position.y = std::fma(cosine, std::fabs(y_max), group.base_position.y);
        }
        return result;
    }

    auto choose_axis = [&](float minimum, float maximum, float base) {
        if (minimum == maximum) return f32_add(base, minimum);
        const float low = std::min(minimum, maximum);
        const float high = std::max(minimum, maximum);
        const int low_integer = static_cast<int>(std::trunc(low));
        const int high_integer = static_cast<int>(std::trunc(high));
        ++result.rng_draws;
        const int offset = choose_inclusive_integer(low_integer, high_integer, random);
        return f32_add(base, static_cast<float>(offset));
    };

    result.position.x = choose_axis(x_min, x_max, group.base_position.x);
    result.position.y = choose_axis(y_min, y_max, group.base_position.y);
    return result;
}

EntityInitialMotionResult choose_initial_member_motion(
    const UnitDefinition& unit,
    const EntityGroupRuntime& group,
    const EntityPoint& member_position,
    bool stationary,
    bool heading_mode,
    int preselected_heading_degrees,
    float velocity_multiplier,
    const EntityInitialMotionFacts& facts,
    LegacyRandom& random,
    const LegacyTrigTables& trig) {
    EntityInitialMotionResult result;
    result.heading_degrees = preselected_heading_degrees;
    (void)group; // Burst/Implode group-vector branch is intentionally deferred below.

    // PPC 0x37B70 returns before initial-speed RNG when stationary.
    if (stationary) return result;

    const float speed_min = unit_float(unit, "initialSpeedMin_FLOAT", 0.0f);
    const float speed_max = unit_float(unit, "initialSpeedMax_FLOAT", speed_min);
    const auto speed_seed = random.seed();
    const float speed = choose_legacy_float(speed_min, speed_max, random);
    if (random.seed() != speed_seed) ++result.rng_draws;

    EntityPoint velocity{};

    // r5 heading-mode is the request/editor path selected by 0x33580.  The
    // member constructor at 0x35EB4 has already jittered the supplied heading
    // before this routine receives it.
    if (heading_mode || unit_bool(unit, "initialHeadingSetInEditor_BOOL")) {
        velocity = legacy_heading_vector(preselected_heading_degrees, speed, trig);
    } else if (unit_bool(unit, "initiallyHuntsClosestPlayer_BOOL")) {
        if (!facts.hunt_target_position) {
            result.status = EntityInitialMotionStatus::missing_hunt_target;
            return result;
        }
        const float dx = static_cast<float>(facts.hunt_target_position->x - member_position.x);
        const float dy = static_cast<float>(facts.hunt_target_position->y - member_position.y);
        const auto direction = legacy_normalize_vector(dx, dy);
        velocity.x = f32_mul(direction.x, speed);
        velocity.y = f32_mul(direction.y, speed);
        // The hunt branch does not overwrite live +0x138 here; later
        // tracking/orientation code can do so. Preserve the constructor's
        // preselected initial heading.
    } else if (unit_bool(unit, "doBurst_BOOL") || unit_bool(unit, "doImplode_BOOL")) {
        // Canonical 1.0.6 has zero definitions using these two paths.  Their
        // vector math is substantially mapped but 0x42CD0's MathLib atan
        // heading conversion is intentionally kept separate until captured.
        result.status = EntityInitialMotionStatus::unsupported_burst_or_implode;
        return result;
    } else {
        int heading = unit_int(unit, "initialHeading_INT", 0);
        bool apply_tolerance = true;

        if (unit_bool(unit, "useOwnerHeading_BOOL") && facts.parent_heading_degrees) {
            heading = *facts.parent_heading_degrees;
            apply_tolerance = false;
        }

        if (apply_tolerance) {
            const int tolerance = unit_int(unit, "initialHeadingTolerance_INT", 0);
            if (tolerance != 0) {
                const int half = tolerance / 2;
                heading += choose_inclusive_integer(-half, half, random);
                ++result.rng_draws;
                heading = wrap_heading_once_or_zero(heading);
            }
        }
        result.heading_degrees = heading;
        velocity = legacy_heading_vector(heading, speed, trig);
    }

    if (velocity_multiplier != 1.0f) {
        velocity.x = f32_mul(velocity.x, velocity_multiplier);
        velocity.y = f32_mul(velocity.y, velocity_multiplier);
    }
    result.velocity_x = velocity.x;
    result.velocity_y = velocity.y;
    return result;
}


void initialize_entity_state_motion(
    EntityRuntime& entity,
    const UnitDefinition& unit,
    std::optional<std::size_t> previous_state_index) {
    const auto state_index = entity.state.current_state;
    if (state_index >= unit.states.size()) {
        throw std::out_of_range("motion current state outside Unit Definition");
    }

    // PPC 0x146F0 clears the motion block for Lock states.  This is distinct
    // from stationary construction: a later state can lock an already-moving
    // member to its owner and therefore must zero both target and delta fields.
    if (state_bool(unit, state_index, "stateLockToOwnerLoc_BOOL")) {
        entity.target_velocity_x = 0.0f;
        entity.target_velocity_y = 0.0f;
        entity.velocity_delta_x = 0.0f;
        entity.velocity_delta_y = 0.0f;
        return;
    }

    int heading = entity.heading_degrees;
    if (previous_state_index && *previous_state_index < unit.states.size() &&
        state_bool(unit, *previous_state_index, "stateOrbitOwner_BOOL")) {
        heading = entity.orbit_angle_degrees;
    } else if (previous_state_index) {
        heading = heading_from_velocity(entity.velocity_x, entity.velocity_y, entity.heading_degrees);
    }
    heading = wrap_heading_once_or_zero(heading);

    static const LegacyTrigTables trig;
    const float max_speed = state_float(unit, state_index, "stateMaxSpeed_FLOAT", 0.0f);
    const float delta = state_float(unit, state_index, "stateDelta_FLOAT", 0.0f);
    const auto target = legacy_heading_vector(heading, max_speed, trig);
    const auto step = legacy_heading_vector(heading, delta, trig);
    entity.target_velocity_x = target.x;
    entity.target_velocity_y = target.y;
    entity.velocity_delta_x = step.x;
    entity.velocity_delta_y = step.y;
}

void advance_entity_hunt_motion(
    EntityRuntime& entity,
    const UnitDefinition& unit,
    LegacyRandom& random) {
    const auto state_index = entity.state.current_state;
    if (state_index >= unit.states.size() ||
        !state_bool(unit, state_index, "stateHunts_BOOL")) return;

    // PPC 0x16FE0 derives a fresh envelope from the Hold maximum-speed field,
    // using two LCG draws in this exact order.  Hunt remains RNG-active even
    // when the closest-player query found no active player.
    const int maximum = static_cast<int>(std::trunc(
        state_float(unit, state_index, "stateHoldMaxSpeed_FLOAT", 0.0f)));
    const int half = maximum / 2;
    const float coarse = static_cast<float>(choose_inclusive_integer(half, maximum, random));
    const float fine = static_cast<float>(choose_inclusive_integer(1, 100, random)) / 100.0f;
    const float envelope = static_cast<float>(coarse + fine);

    auto hunt_axis = [&](float& velocity, float& delta) {
        if (velocity > envelope) {
            velocity = envelope;
            delta = -std::fabs(delta);
        } else if (velocity < -envelope) {
            velocity = -envelope;
            delta = std::fabs(delta);
        }
        velocity = static_cast<float>(velocity + delta);
    };
    hunt_axis(entity.velocity_x, entity.velocity_delta_x);
    hunt_axis(entity.velocity_y, entity.velocity_delta_y);
    entity.target_velocity_x = entity.velocity_x;
    entity.target_velocity_y = entity.velocity_y;
}

void advance_entity_hold_motion(
    EntityRuntime& entity,
    const UnitDefinition& unit) {
    const auto state_index = entity.state.current_state;
    if (state_index >= unit.states.size() ||
        !state_bool(unit, state_index, "stateHoldPositionToTarget_BOOL") ||
        !entity.has_active_target || entity.target_player_distance <= 0.0f) return;

    const float max_speed = state_float(unit, state_index, "stateHoldMaxSpeed_FLOAT", 0.0f);
    const float dx = static_cast<float>(entity.target_player_x - entity.x);
    const float dy = static_cast<float>(entity.target_player_y - entity.y);
    const float inv_distance = static_cast<float>(1.0f / entity.target_player_distance);
    // PPC 0x17C40 deliberately negates the normalized target displacement.
    entity.target_velocity_x = static_cast<float>(-dx * inv_distance * max_speed);
    entity.target_velocity_y = static_cast<float>(-dy * inv_distance * max_speed);

    const float hold_delta = state_float(unit, state_index, "stateHoldDelta_FLOAT", 0.0f);
    const float magnitude = std::sqrt(static_cast<float>(dx * dx + dy * dy));
    if (magnitude > 0.0f) {
        entity.velocity_delta_x = static_cast<float>(std::fabs(dx / magnitude * hold_delta));
        entity.velocity_delta_y = static_cast<float>(std::fabs(dy / magnitude * hold_delta));
    }
}

void advance_entity_cyclic_motion(
    EntityRuntime& entity,
    const UnitDefinition& unit) {
    const auto state_index = entity.state.current_state;
    if (state_index >= unit.states.size() ||
        !state_bool(unit, state_index, "stateCyclicMotion_BOOL") ||
        !entity.has_active_target) return;

    const float maximum = std::fabs(state_float(unit, state_index, "stateMaxSpeed_FLOAT", 0.0f));
    const float delta = std::fabs(state_float(unit, state_index, "stateDelta_FLOAT", 0.0f));
    entity.velocity_delta_x = entity.x < entity.target_player_x ? delta : -delta;
    entity.velocity_delta_y = entity.y < entity.target_player_y ? delta : -delta;
    entity.velocity_x = std::clamp(static_cast<float>(entity.velocity_x + entity.velocity_delta_x), -maximum, maximum);
    entity.velocity_y = std::clamp(static_cast<float>(entity.velocity_y + entity.velocity_delta_y), -maximum, maximum);
    entity.target_velocity_x = entity.velocity_x;
    entity.target_velocity_y = entity.velocity_y;
}

void advance_entity_flee_motion(
    EntityRuntime& entity,
    const UnitDefinition& unit) {
    const auto state_index = entity.state.current_state;
    if (state_index >= unit.states.size() || !entity.fleeing || !entity.has_active_target) return;

    const float maximum = std::fabs(state_float(unit, state_index, "stateFleeSpeed_FLOAT", 0.0f));
    const float delta = std::fabs(state_float(unit, state_index, "stateFleeDelta_FLOAT", 0.0f));
    entity.velocity_delta_x = entity.x < entity.target_player_x ? -delta : delta;
    entity.velocity_delta_y = entity.y < entity.target_player_y ? -delta : delta;
    entity.velocity_x = std::clamp(static_cast<float>(entity.velocity_x + entity.velocity_delta_x), -maximum, maximum);
    entity.velocity_y = std::clamp(static_cast<float>(entity.velocity_y + entity.velocity_delta_y), -maximum, maximum);
    entity.target_velocity_x = entity.velocity_x;
    entity.target_velocity_y = entity.velocity_y;
}

void converge_entity_velocity(
    EntityRuntime& entity,
    const UnitDefinition& unit) {
    if (entity.stationary) {
        entity.velocity_x = 0.0f;
        entity.velocity_y = 0.0f;
        entity.target_velocity_x = 0.0f;
        entity.target_velocity_y = 0.0f;
        entity.velocity_delta_x = 0.0f;
        entity.velocity_delta_y = 0.0f;
        return;
    }

    const auto state_index = entity.state.current_state;
    if (state_index >= unit.states.size()) return;
    entity.velocity_x = approach_axis(entity.velocity_x, entity.target_velocity_x, entity.velocity_delta_x);
    entity.velocity_y = approach_axis(entity.velocity_y, entity.target_velocity_y, entity.velocity_delta_y);
}

int append_group_member_delay(
    const UnitDefinition& unit,
    int accumulated_delay,
    LegacyRandom& random) {
    const int minimum = unit_int(unit, "groupDelayMin_INT", 0);
    const int maximum = unit_int(unit, "groupDelayMax_INT", minimum);
    return accumulated_delay + choose_inclusive_integer(minimum, maximum, random);
}

int choose_initial_member_heading(
    const UnitDefinition& unit,
    bool heading_mode,
    int supplied_heading_degrees,
    LegacyRandom& random) {
    if (!heading_mode) {
        return unit_int(unit, "initialHeading_INT", 0);
    }

    int heading = supplied_heading_degrees;
    const int tolerance = unit_int(unit, "initialHeadingTolerance_INT", 0);
    if (tolerance != 0) {
        // PPC 0x35ED4..0x35EE4 computes a signed half-width using arithmetic
        // divide-by-two semantics, then calls inclusive RNG on -half..+half.
        const int half = tolerance / 2;
        heading += choose_inclusive_integer(-half, half, random);
    }
    return wrap_heading_once_or_zero(heading);
}

void enter_entity_state(
    EntityRuntime& entity,
    const UnitDefinition& unit,
    std::size_t state_index,
    std::uint32_t current_tick,
    LegacyRandom& random) {
    enter_entity_state_impl(entity, unit, state_index, current_tick, random, 0);
}

void initialize_entity_state_machine(
    EntityRuntime& entity,
    const UnitDefinition& unit,
    std::uint32_t current_tick,
    LegacyRandom& random) {
    if (unit.states.empty()) {
        throw std::invalid_argument("cannot initialize entity state machine from Unit Definition with no states");
    }
    entity.behavior = compile_unit_behavior(unit);
    entity.spawn_runtime_by_state.clear();
    entity.spawn_runtime_by_state.resize(unit.states.size());
    entity.lifecycle = EntityLifecycle::active;
    enter_entity_state(entity, unit, 0, current_tick, random);
}

void apply_entity_state_action(
    EntityRuntime& entity,
    const UnitDefinition& unit,
    const ResolvedStateAction& action,
    std::uint32_t current_tick,
    LegacyRandom& random) {
    if (entity.lifecycle != EntityLifecycle::active) return;

    switch (action.kind) {
    case StateActionKind::none:
    case StateActionKind::unresolved:
        return;
    case StateActionKind::delete_entity:
        entity.lifecycle = EntityLifecycle::deleted;
        return;
    case StateActionKind::destroy_entity:
        entity.lifecycle = EntityLifecycle::destroyed;
        return;
    case StateActionKind::change_state:
        enter_entity_state(entity, unit, action.state_index, current_tick, random);
        return;
    }
}

} // namespace deimos

namespace deimos {

EntityGroupBuildResult construct_entity_group_headless(
    const UnitDefinition& unit,
    const SpawnRequestSeed& request,
    const EntityHeadlessConstructionContext& context,
    EntityIdentityCounters& identities,
    LegacyRandom& random,
    const LegacyTrigTables& trig) {
    EntityGroupBuildResult result;
    result.plan = prepare_entity_group_construction(unit, request, context.preflight, random);
    if (!result.plan.accepted()) {
        result.status = EntityGroupBuildStatus::rejected;
        return result;
    }

    const std::uint32_t group_serial = identities.next_group_serial++;
    result.group = build_entity_group_runtime(
        request, result.plan.group.surviving_members, group_serial, context.world_y_origin);

    const bool heading_mode = request.heading_is_set ||
        unit_bool(unit, "initialHeadingSetInEditor_BOOL");
    const int supplied_heading = request.heading_is_set
        ? request.heading_degrees
        : request.editor_heading_degrees;

    int accumulated_group_delay = 0;
    result.members.reserve(static_cast<std::size_t>(result.plan.group.surviving_members));

    for (int i = 0; i < result.plan.group.surviving_members; ++i) {
        EntityRuntime member;
        member.handle = identities.next_member_handle++;
        member.serial = identities.next_member_serial++;
        member.group_serial = group_serial;
        member.parent = request.parent;
        member.player_owner_index = request.player_owner_index;
        member.unit_id = request.unit_id;
        member.stationary = request.stationary;
        member.terrain_effects_enabled = request.terrain_effects_enabled;

        // 0x35EB4 runs before initial position/motion and may consume the
        // tolerance draw for request/editor headings even for stationary units.
        member.heading_degrees = choose_initial_member_heading(
            unit, heading_mode, supplied_heading, random);

        const auto position = choose_initial_member_position(unit, *result.group, random, trig);
        member.x = position.position.x;
        member.y = position.position.y;

        auto member_motion_facts = context.motion_facts;
        if (unit_bool(unit, "initiallyHuntsClosestPlayer_BOOL") && context.hunt_target_provider) {
            member_motion_facts.hunt_target_position = context.hunt_target_provider(position.position);
        }
        const auto motion = choose_initial_member_motion(
            unit, *result.group, position.position, request.stationary,
            heading_mode, member.heading_degrees,
            request.initial_velocity_multiplier, member_motion_facts,
            random, trig);
        if (motion.status == EntityInitialMotionStatus::missing_hunt_target) {
            result.status = EntityGroupBuildStatus::missing_hunt_target;
            return result;
        }
        if (motion.status == EntityInitialMotionStatus::unsupported_burst_or_implode) {
            result.status = EntityGroupBuildStatus::unsupported_burst_or_implode;
            return result;
        }
        member.heading_degrees = motion.heading_degrees;
        member.velocity_x = motion.velocity_x;
        member.velocity_y = motion.velocity_y;

        // PPC 0x35F58 enters the first state and initializes state spawn
        // records before groupDelay RNG at 0x35FC8.
        initialize_entity_state_machine(member, unit, context.preflight.current_tick, random);
        accumulated_group_delay = append_group_member_delay(unit, accumulated_group_delay, random);
        member.group_delay_ticks = accumulated_group_delay;

        if (i == 0) {
            result.first_member_reference = {member.handle, member.serial};
        }
        result.members.push_back(std::move(member));
    }

    result.status = EntityGroupBuildStatus::complete;
    return result;
}

EntityTickResult advance_entity_runtime(
    EntityRuntime& entity,
    const UnitDefinition& unit,
    const EntityTickContext& context,
    LegacyRandom& random) {
    EntityTickResult result;
    if (entity.lifecycle != EntityLifecycle::active) return result;
    if (entity.state.current_state >= entity.behavior.states.size() ||
        entity.state.current_state >= unit.states.size()) {
        throw std::out_of_range("live entity current state outside Unit Definition");
    }

    // Main entity update around PPC 0x33C58: timer is checked before the
    // animation/rule portion.  The state is re-read after applying the action.
    if (state_timer_due(entity.state, context.current_tick)) {
        const auto state_index = entity.state.current_state;
        const auto action = entity.behavior.states[state_index].on_timer;
        result.timer_action_processed = true;
        apply_entity_state_action(entity, unit, action, context.current_tick, random);
        if (entity.lifecycle != EntityLifecycle::active) return result;
    }

    // Animation processing sits here in the original.  Until sprite animation
    // is reconstructed, the caller supplies facts representing the post-
    // animation state used by the rule predicates.
    if (context.facts_for_rule) {
        const auto state_index = entity.state.current_state;
        const auto& state = entity.behavior.states[state_index];
        const auto evaluation = evaluate_first_matching_rule(state, context.facts_for_rule);
        if (evaluation.matched) {
            result.rule_matched = true;
            apply_entity_state_action(
                entity, unit, evaluation.action, context.current_tick, random);
            if (entity.lifecycle != EntityLifecycle::active) return result;
        }
    }

    // PPC 0x15280 refreshes target-player facts and executes Hunt/no-player
    // lifecycle work here, before range handling.  Use the just-refreshed
    // distance when the world phase supplies one; otherwise preserve the older
    // explicit range-input hook for isolated state-machine tests.
    std::optional<float> measured_range = context.measured_player_range;
    if (context.pre_range_motion_phase) {
        const auto refreshed = context.pre_range_motion_phase(entity);
        if (refreshed) measured_range = refreshed;
        if (entity.lifecycle != EntityLifecycle::active) return result;
    }

    // PPC range handling occurs later than rules.  A state change above means
    // this check intentionally uses the newly current state.
    if (measured_range) {
        const auto state_index = entity.state.current_state;
        const auto& state = entity.behavior.states[state_index];
        if (state_range_transition_due(state.range, *measured_range)) {
            result.range_action_processed = true;
            apply_entity_state_action(entity, unit, state.on_range, context.current_tick, random);
            if (entity.lifecycle != EntityLifecycle::active) return result;
        }
    }

    // After a possible range state change, 0x15280 reloads current state and
    // performs Hold/Cyclic/Flee/convergence before owner-location behavior.
    if (context.post_range_motion_phase) {
        context.post_range_motion_phase(entity);
        if (entity.lifecycle != EntityLifecycle::active) return result;
    }

    // PPC 0x3401C..0x34054: owner-location Lock/Link/Orbit is evaluated
    // after range handling and before the spawn scheduler at 0x15B40. State
    // changes above have invalidated the owner-location state marker, allowing
    // the world phase to run the 0x33600 initializer for the newly current state.
    if (context.owner_location_phase) {
        context.owner_location_phase(entity);
        if (entity.lifecycle != EntityLifecycle::active) return result;
    }

    // Spawn scheduling is later in the recovered entity path.  Any state
    // transition earlier in this tick therefore selects that state's freshly
    // initialized spawn records.
    const auto state_index = entity.state.current_state;
    if (state_index >= entity.spawn_runtime_by_state.size()) {
        throw std::out_of_range("live entity spawn-runtime state outside Unit Definition");
    }
    const auto& state_definition = unit.states[state_index];
    auto& spawn_state = entity.spawn_runtime_by_state[state_index].spawn_sets;
    if (spawn_state.size() != state_definition.spawn_sets.size()) {
        throw std::runtime_error("live entity spawn-runtime count does not match Unit Definition");
    }

    auto schedule_context = context.spawn_schedule;
    schedule_context.current_rotation_pause_ticks = entity.rotation_pause_ticks;
    for (std::size_t i = 0; i < state_definition.spawn_sets.size(); ++i) {
        auto step = advance_spawn_set_schedule(
            state_definition.spawn_sets[i], spawn_state[i],
            context.current_tick, schedule_context, random);
        if (step.rotation_pause_updated) {
            entity.rotation_pause_ticks = step.requested_rotation_pause_ticks;
            schedule_context.current_rotation_pause_ticks = entity.rotation_pause_ticks;
        }
        if (step.spawn_due) result.spawns_due.push_back({state_index, i});
    }

    return result;
}

} // namespace deimos
