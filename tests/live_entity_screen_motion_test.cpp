#include "deimos/live_entity_screen_motion.hpp"

#include <cassert>
#include <iostream>

int main() {
    deimos::EntityRuntime projectile;
    projectile.x = 208.0f;
    projectile.y = 330.0f;
    projectile.velocity_x = 0.0f;
    projectile.velocity_y = 10.0f; // canonical heading 0 / north
    deimos::advance_live_entity_screen_position(projectile);
    assert(projectile.x == 208.0f);
    assert(projectile.y == 320.0f);

    deimos::EntityRuntime south;
    south.x = 100.0f;
    south.y = 200.0f;
    south.velocity_y = -3.5f; // canonical heading 180 / south
    deimos::advance_live_entity_screen_position(south);
    assert(south.y == 203.5f);

    deimos::shift_live_entity_for_terrain_scroll(south, 1);
    assert(south.y == 204.5f);

    std::cout << "live entity screen motion PASS\n";
    return 0;
}
