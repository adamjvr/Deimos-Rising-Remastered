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

} // namespace deimos
