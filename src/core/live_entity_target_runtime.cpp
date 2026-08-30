#include "deimos/live_entity_target_runtime.hpp"

#include "deimos/legacy_math.hpp"

#include <algorithm>
#include <cmath>

namespace deimos {
namespace {

int wrap_heading(int heading) {
    heading %= 360;
    if (heading < 0) heading += 360;
    return heading;
}

} // namespace

int live_entity_state_base_frame(
    const EntityRuntime& entity,
    const UnitDefinition& unit) {
    if (entity.state.current_state >= unit.states.size()) return 0;
    return unit.states[entity.state.current_state]
        .fields.int_value("stateSpriteFrameMin_INT")
        .value_or(0);
}

int legacy_direction_frame_for_heading(
    int heading_degrees,
    int direction_count) {
    if (direction_count <= 1) return 0;
    const int heading = wrap_heading(heading_degrees);
    const int frame = (heading * direction_count) / 360;
    return std::clamp(frame, 0, direction_count - 1);
}

LiveEntityTargetFacingResult advance_live_entity_target_facing(
    EntityRuntime& entity,
    const UnitDefinition& unit,
    LegacySpriteVisualRuntime& visual) {
    LiveEntityTargetFacingResult out;
    out.heading_before = entity.heading_degrees;
    out.heading_after = entity.heading_degrees;
    out.frame_before = visual.sprite_frame;
    out.frame_after = visual.sprite_frame;

    if (entity.state.current_state >= unit.states.size()) return out;
    const auto& fields = unit.states[entity.state.current_state].fields;
    out.rotate_to_target = fields.bool_value("stateDoRotateToTarget_BOOL").value_or(false);
    out.target_available = entity.has_active_target;

    if (!out.rotate_to_target || !out.target_available || entity.rotation_pause_ticks > 0) {
        return out;
    }

    const int directions = fields.int_value("stateNumDirections_INT").value_or(1);
    const int frames_per_direction = fields.int_value("stateFramesPerDirection_INT").value_or(1);
    // Canonical 1.0.6 rotate-to-target states are uniformly 36x1. Fail
    // conservatively for foreign layouts rather than guessing atlas packing.
    if (directions <= 1 || frames_per_direction != 1) return out;

    const int ex = static_cast<int>(std::trunc(entity.x));
    const int ey = static_cast<int>(std::trunc(entity.y));
    const int tx = static_cast<int>(std::trunc(entity.target_player_x));
    const int ty = static_cast<int>(std::trunc(entity.target_player_y));
    const int heading = legacy_angle_between_integer_points(ex, ey, tx, ty);

    entity.heading_degrees = heading;
    visual.sprite_frame = legacy_direction_frame_for_heading(heading, directions);
    visual.bounds_dirty = true;

    out.applied = true;
    out.heading_after = entity.heading_degrees;
    out.frame_after = visual.sprite_frame;
    return out;
}

} // namespace deimos
