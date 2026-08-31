#include "deimos/live_entity_screen_motion.hpp"

namespace deimos {

void shift_live_entity_for_terrain_scroll(EntityRuntime& entity, int applied_vertical_delta) {
    entity.y += static_cast<float>(applied_vertical_delta);
}

void advance_live_entity_screen_position(EntityRuntime& entity) {
    entity.x += entity.velocity_x;
    entity.y -= entity.velocity_y;
}

bool legacy_live_entity_within_main_tick_bounds(
    const EntityRuntime& entity,
    int visible_width,
    int visible_height,
    int margin) {
    const float half_width = static_cast<float>(entity.collision_half_width);
    const float half_height = static_cast<float>(entity.collision_half_height);
    const float left_limit = -static_cast<float>(margin);
    const float right_limit = static_cast<float>(visible_width + margin);
    const float top_limit = -static_cast<float>(margin);
    const float bottom_limit = static_cast<float>(visible_height + margin);

    return entity.x + half_width >= left_limit &&
           entity.x - half_width <= right_limit &&
           entity.y >= top_limit &&
           entity.y - half_height <= bottom_limit;
}

} // namespace deimos
