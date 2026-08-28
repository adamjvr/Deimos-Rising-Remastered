#include "deimos/terrain_runtime.hpp"

#include <cassert>
#include <iostream>
#include <string>

namespace {

deimos::FourCC id(const char* s) {
    return deimos::FourCC{{s[0], s[1], s[2], s[3]}};
}

deimos::EntityRuntime ground_entity() {
    deimos::EntityRuntime e;
    e.x = 10.75f;
    e.y = -4.75f;
    e.collision_half_width = 3;
    e.collision_half_height = 5;
    e.behavior.collision_domain = id("grnd");
    return e;
}

} // namespace

int main() {
    // PPC 0x2A6D0/0x2A830: append-only rectangles and inclusive edge overlap.
    deimos::LegacyGroundObstacleRects obstacles;
    obstacles.add({10, 20, 30, 40});
    obstacles.add({100, 200, 120, 220});
    assert(obstacles.size() == 2);
    assert(obstacles.overlaps({30, 40, 35, 45})); // touching corner counts
    assert(!obstacles.overlaps({31, 41, 35, 45}));

    // PPC 0x2A770 shifts raw QuickDraw top/bottom, i.e. semantic Y only.
    obstacles.shift_vertical(-7);
    assert((obstacles.rects()[0] == deimos::RectI{10, 13, 30, 33}));
    assert((obstacles.rects()[1] == deimos::RectI{100, 193, 120, 213}));

    auto e = ground_entity();
    e.behavior.collides_with_ground_obstacles = false;
    const auto entity_rect = deimos::legacy_entity_world_rect(e);
    assert((entity_rect == deimos::RectI{7, -9, 13, 0}));
    deimos::LegacyGroundObstacleRects entity_obstacle;
    entity_obstacle.add(entity_rect);
    assert(!deimos::legacy_collides_with_ground_obstacle(e, entity_obstacle));
    e.behavior.collides_with_ground_obstacles = true;
    assert(deimos::legacy_collides_with_ground_obstacle(e, entity_obstacle));

    // Main-tick consequence around 0x34538: obstacle contact does NOT restore
    // position. It copies the shared canonical zero vector to velocity and
    // latches stationary. draw-to-terrain appends the stopped Rect afterward.
    e.velocity_x = 3.25f;
    e.velocity_y = -1.5f;
    e.stationary = false;
    e.behavior.destruction_draw_to_terrain = true;
    const float x_before_stop = e.x;
    const float y_before_stop = e.y;
    const auto obstacle_count_before_stop = entity_obstacle.size();
    assert(deimos::apply_legacy_ground_obstacle_stop(e, entity_obstacle));
    assert(e.x == x_before_stop && e.y == y_before_stop);
    assert(e.velocity_x == 0.0f && e.velocity_y == 0.0f);
    assert(e.stationary);
    assert(entity_obstacle.size() == obstacle_count_before_stop + 1);
    assert(!deimos::apply_legacy_ground_obstacle_stop(e, entity_obstacle));

    // Objects[gaob] slots 6..9 are a positional binary contract.
    deimos::NamedTable<deimos::FourCC> objects(10);
    for (std::size_t i = 0; i < objects.size(); ++i) objects[i] = {"unused", id("none")};
    objects[6] = {"MediaImpact_Water_Tiny", id("spti")};
    objects[7] = {"MediaImpact_Water_Small", id("spsm")};
    objects[8] = {"MediaImpact_Water_Medium", id("spme")};
    objects[9] = {"MediaImpact_Water_Large", id("spla")};
    std::string error;
    const auto config = deimos::compile_legacy_water_impact_config(objects, &error);
    assert(config);
    assert(config->tiny == id("spti"));
    assert(config->small == id("spsm"));
    assert(config->medium == id("spme"));
    assert(config->large == id("spla"));
    objects[8].first = "wrong";
    assert(!deimos::compile_legacy_water_impact_config(objects, &error));

    int probe_calls = 0;
    int seen_x = 0;
    int seen_y = 0;
    auto water = [&](int x, int y) {
        ++probe_calls;
        seen_x = x;
        seen_y = y;
        return true;
    };

    // Non-ground bypasses the Media Mask entirely.
    auto air = e;
    air.behavior.collision_domain = id("air ");
    air.behavior.media_impact_size = id("smal");
    deimos::LegacyRandom rng_air(123);
    const auto air_seed = rng_air.seed();
    const auto air_result = deimos::resolve_legacy_removal_media(
        air, 500, *config, water, rng_air);
    assert(air_result.allow_requested_spawn);
    assert(!air_result.water_hit);
    assert(probe_calls == 0);
    assert(rng_air.seed() == air_seed);

    // The explicit source flag is the second pre-mask bypass.
    e.behavior.death_spawn_on_any_media = true;
    deimos::LegacyRandom rng_any(124);
    const auto any_result = deimos::resolve_legacy_removal_media(
        e, 500, *config, water, rng_any);
    assert(any_result.allow_requested_spawn);
    assert(probe_calls == 0);
    e.behavior.death_spawn_on_any_media = false;

    // 0x168C4..0x168F8 truncates first, then adds +32 / world-Y origin.
    e.behavior.media_impact_size = id("smal");
    deimos::LegacyRandom rng_small(125);
    const auto small_result = deimos::resolve_legacy_removal_media(
        e, 500, *config, water, rng_small);
    assert(probe_calls == 1);
    assert(seen_x == 42); // trunc(10.75) + 32
    assert(seen_y == 496); // trunc(-4.75) + 500
    assert(small_result.water_hit);
    assert(!small_result.allow_requested_spawn);
    assert(small_result.replacement_spawn == id("spsm"));
    assert(small_result.rng_draws == 0);

    // A water hit suppresses the requested spawn even when no replacement
    // media-impact size is configured.
    e.behavior.media_impact_size = id("none");
    deimos::LegacyRandom rng_none(126);
    const auto none_result = deimos::resolve_legacy_removal_media(
        e, 500, *config, water, rng_none);
    assert(none_result.water_hit);
    assert(!none_result.allow_requested_spawn);
    assert(!none_result.replacement_spawn);

    // Non-water allows the caller's original death/deletion spawn and consumes
    // no media-impact RNG even for a random size selector.
    e.behavior.media_impact_size = id("smra");
    deimos::LegacyRandom rng_dry(127);
    const auto dry_seed = rng_dry.seed();
    const auto dry_result = deimos::resolve_legacy_removal_media(
        e, 500, *config, [](int, int) { return false; }, rng_dry);
    assert(!dry_result.water_hit);
    assert(dry_result.allow_requested_spawn);
    assert(!dry_result.replacement_spawn);
    assert(rng_dry.seed() == dry_seed);

    // Randomized enum ordering is quirky and must preserve the original draw.
    e.behavior.media_impact_size = id("smra");
    deimos::LegacyRandom expected_smra(200);
    const int smra_roll = deimos::choose_inclusive_integer(0, 1, expected_smra);
    deimos::LegacyRandom actual_smra(200);
    const auto smra = deimos::resolve_legacy_removal_media(
        e, 500, *config, water, actual_smra);
    assert(smra.rng_draws == 1);
    assert(smra.replacement_spawn == (smra_roll == 0 ? id("spsm") : id("spti")));
    assert(actual_smra.seed() == expected_smra.seed());

    e.behavior.media_impact_size = id("mera");
    deimos::LegacyRandom expected_mera(201);
    const int mera_roll = deimos::choose_inclusive_integer(0, 2, expected_mera);
    deimos::LegacyRandom actual_mera(201);
    const auto mera = deimos::resolve_legacy_removal_media(
        e, 500, *config, water, actual_mera);
    const auto mera_expected = mera_roll == 0 ? id("spti") :
        (mera_roll == 1 ? id("spsm") : id("spme"));
    assert(mera.replacement_spawn == mera_expected);
    assert(actual_mera.seed() == expected_mera.seed());

    e.behavior.media_impact_size = id("lara");
    deimos::LegacyRandom expected_lara(202);
    const int lara_roll = deimos::choose_inclusive_integer(0, 1, expected_lara);
    deimos::LegacyRandom actual_lara(202);
    const auto lara = deimos::resolve_legacy_removal_media(
        e, 500, *config, water, actual_lara);
    assert(lara.replacement_spawn == (lara_roll == 0 ? id("spla") : id("spme")));
    assert(actual_lara.seed() == expected_lara.seed());

    // Unknown size IDs follow the switch default: water suppresses the original
    // spawn but the helper emits no replacement and consumes no RNG.
    e.behavior.media_impact_size = id("????");
    deimos::LegacyRandom rng_unknown(203);
    const auto unknown_seed = rng_unknown.seed();
    const auto unknown = deimos::resolve_legacy_removal_media(
        e, 500, *config, water, rng_unknown);
    assert(unknown.water_hit && !unknown.allow_requested_spawn);
    assert(!unknown.replacement_spawn);
    assert(rng_unknown.seed() == unknown_seed);

    obstacles.clear();
    assert(obstacles.size() == 0);

    std::cout << "terrain/media runtime tests passed\n";
    return 0;
}
