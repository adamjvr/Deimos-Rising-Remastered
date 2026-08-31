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

    // Direct PPC-Lab execution of shipped 0x12CA0 with the main-tick
    // margin=128 and viewport=416x480. With half extents 10x10, equality at
    // every surviving edge is intentional. The top edge tests the member
    // origin directly; the other three tested edges use the proven extents.
    deimos::EntityRuntime bounded;
    bounded.collision_half_width = 10;
    bounded.collision_half_height = 10;
    bounded.y = 100.0f;
    bounded.x = -139.0f;
    assert(!deimos::legacy_live_entity_within_main_tick_bounds(bounded, 416, 480));
    bounded.x = -138.0f;
    assert(deimos::legacy_live_entity_within_main_tick_bounds(bounded, 416, 480));
    bounded.x = 554.0f;
    assert(deimos::legacy_live_entity_within_main_tick_bounds(bounded, 416, 480));
    bounded.x = 555.0f;
    assert(!deimos::legacy_live_entity_within_main_tick_bounds(bounded, 416, 480));

    bounded.x = 100.0f;
    bounded.y = -129.0f;
    assert(!deimos::legacy_live_entity_within_main_tick_bounds(bounded, 416, 480));
    bounded.y = -128.0f;
    assert(deimos::legacy_live_entity_within_main_tick_bounds(bounded, 416, 480));
    bounded.y = 618.0f;
    assert(deimos::legacy_live_entity_within_main_tick_bounds(bounded, 416, 480));
    bounded.y = 619.0f;
    assert(!deimos::legacy_live_entity_within_main_tick_bounds(bounded, 416, 480));

    std::cout << "live entity screen motion PASS\n";
    return 0;
}
