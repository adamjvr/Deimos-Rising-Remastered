#pragma once

#include "deimos/entity_runtime.hpp"

namespace deimos {

// Visible-screen integration seam used by the first original-data live world.
// The sign is strongly corroborated by canonical corpus semantics: heading 0
// creates +Y velocity and Player-1 projectiles with heading 0 travel north,
// while heading 180 creates -Y velocity and south-moving enemy definitions
// travel down-screen. The exact outer PPC integrator remains a separate
// instruction-closure item, so this helper is intentionally named as a screen
// integration contract rather than claiming a recovered function address.
void shift_live_entity_for_terrain_scroll(EntityRuntime& entity, int applied_vertical_delta);
void advance_live_entity_screen_position(EntityRuntime& entity);

// Shipped 1.0.6 PPC 0x12CA0 performs the main live-member movement and then
// returns a bounded-lifetime predicate. The main entity tick calls it with
// margin=128 (r4) and the viewport dimensions from Game.gafl[54]/[55]. PPC
// Lab boundary execution proves the post-movement inequalities are asymmetric:
// left includes half-width, right includes half-width, top tests the entity
// origin directly, and bottom includes half-height. Equality survives.
[[nodiscard]] bool legacy_live_entity_within_main_tick_bounds(
    const EntityRuntime& entity,
    int visible_width,
    int visible_height,
    int margin = 128);

} // namespace deimos
