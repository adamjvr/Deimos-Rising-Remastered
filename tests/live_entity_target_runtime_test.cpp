#include "deimos/live_entity_target_runtime.hpp"

#include <cassert>

namespace {
deimos::UnitDefinition rotating_unit() {
    deimos::UnitDefinition unit;
    deimos::UnitStateDefinition state;
    state.name = "Track";
    state.fields.add({"stateSpriteFrameMin_INT", 18, "18", 1});
    state.fields.add({"stateDoRotateToTarget_BOOL", true, "TRUE", 2});
    state.fields.add({"stateNumDirections_INT", 36, "36", 3});
    state.fields.add({"stateFramesPerDirection_INT", 1, "1", 4});
    unit.states.push_back(std::move(state));
    return unit;
}
}

int main() {
    assert(deimos::legacy_direction_frame_for_heading(0, 36) == 0);
    assert(deimos::legacy_direction_frame_for_heading(90, 36) == 9);
    assert(deimos::legacy_direction_frame_for_heading(180, 36) == 18);
    assert(deimos::legacy_direction_frame_for_heading(270, 36) == 27);
    assert(deimos::legacy_direction_frame_for_heading(359, 36) == 35);

    const auto unit = rotating_unit();
    deimos::EntityRuntime entity;
    entity.state.current_state = 0;
    entity.x = 100.0f;
    entity.y = 100.0f;
    entity.has_active_target = true;
    deimos::LegacySpriteVisualRuntime visual;
    visual.sprite_frame = 18;

    entity.target_player_x = 100.0f;
    entity.target_player_y = 0.0f;
    auto north = deimos::advance_live_entity_target_facing(entity, unit, visual);
    assert(north.applied && entity.heading_degrees == 0 && visual.sprite_frame == 0);

    entity.target_player_x = 200.0f;
    entity.target_player_y = 100.0f;
    auto east = deimos::advance_live_entity_target_facing(entity, unit, visual);
    assert(east.applied && entity.heading_degrees == 90 && visual.sprite_frame == 9);

    entity.target_player_x = 100.0f;
    entity.target_player_y = 200.0f;
    auto south = deimos::advance_live_entity_target_facing(entity, unit, visual);
    assert(south.applied && entity.heading_degrees == 180 && visual.sprite_frame == 18);

    entity.target_player_x = 0.0f;
    entity.target_player_y = 100.0f;
    auto west = deimos::advance_live_entity_target_facing(entity, unit, visual);
    assert(west.applied && entity.heading_degrees == 270 && visual.sprite_frame == 27);

    entity.rotation_pause_ticks = 5;
    visual.sprite_frame = 18;
    entity.heading_degrees = 180;
    entity.target_player_x = 200.0f;
    auto paused = deimos::advance_live_entity_target_facing(entity, unit, visual);
    assert(!paused.applied);
    assert(entity.heading_degrees == 180 && visual.sprite_frame == 18);

    assert(deimos::live_entity_state_base_frame(entity, unit) == 18);
    return 0;
}
