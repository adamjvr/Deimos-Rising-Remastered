#pragma once

#include "deimos/entity_runtime.hpp"
#include "deimos/render_runtime.hpp"
#include "deimos/unit_definition.hpp"

namespace deimos {

// First recovered target-facing bridge for live entity visuals. Canonical
// 1.0.6 uses stateDoRotateToTarget_BOOL only with 36 directions and one frame
// per direction. Heading 0/90/180/270 maps to frame 0/9/18/27 respectively,
// matching the original directional atlas layout.
struct LiveEntityTargetFacingResult {
    bool rotate_to_target = false;
    bool target_available = false;
    bool applied = false;
    int heading_before = 0;
    int heading_after = 0;
    int frame_before = 0;
    int frame_after = 0;
};

[[nodiscard]] int live_entity_state_base_frame(
    const EntityRuntime& entity,
    const UnitDefinition& unit);

[[nodiscard]] int legacy_direction_frame_for_heading(
    int heading_degrees,
    int direction_count);

// Uses the already-recovered PPC integer-point angle helper so target-facing
// remains in the game's own heading convention instead of host atan2 space.
// Rotation pause is honored. This routine updates both the live heading and
// the visual frame; it does not invent velocity steering.
[[nodiscard]] LiveEntityTargetFacingResult advance_live_entity_target_facing(
    EntityRuntime& entity,
    const UnitDefinition& unit,
    LegacySpriteVisualRuntime& visual);

} // namespace deimos
