#include "deimos/live_entity_screen_motion.hpp"

namespace deimos {

void shift_live_entity_for_terrain_scroll(EntityRuntime& entity, int applied_vertical_delta) {
    entity.y += static_cast<float>(applied_vertical_delta);
}

void advance_live_entity_screen_position(EntityRuntime& entity) {
    entity.x += entity.velocity_x;
    entity.y -= entity.velocity_y;
}

} // namespace deimos
