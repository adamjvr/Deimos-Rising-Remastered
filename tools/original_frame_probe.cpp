#include "deimos/original_game_frame_preview.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <span>
#include <string>

namespace {
std::uint64_t fnv1a64(std::span<const std::uint16_t> pixels) {
    std::uint64_t h = 1469598103934665603ull;
    for (const auto v : pixels) {
        h ^= static_cast<std::uint8_t>(v & 0xffu);
        h *= 1099511628211ull;
        h ^= static_cast<std::uint8_t>((v >> 8u) & 0xffu);
        h *= 1099511628211ull;
    }
    return h;
}
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: deimos_original_frame_probe /path/to/Paks\n";
        return 2;
    }

    std::string error;
    auto preview = deimos::OriginalGameFramePreview::load(
        std::filesystem::path(argv[1]), {{'l','e','0','1'}}, 0, &error);
    if (!preview) {
        std::cerr << "original frame load failed: " << error << '\n';
        return 3;
    }

    deimos::LegacyRasterSurface frame;
    deimos::LegacyGameplayFrameResult result;
    if (!preview->render(frame, &result, &error)) {
        std::cerr << "original frame render failed: " << error << '\n';
        return 4;
    }

    const auto initial_hash = fnv1a64(frame.pixels);
    if (initial_hash != deimos::kCanonicalOriginalGameInitialFrameFnv64) {
        std::cerr << "original frame oracle mismatch: got 0x" << std::hex << initial_hash
                  << " expected 0x" << deimos::kCanonicalOriginalGameInitialFrameFnv64
                  << std::dec << '\n';
        return 5;
    }

    const auto& info = preview->info();
    if (info.media_mask_id != deimos::FourCC{{'c','a','t','1'}} ||
        info.media_mask_width != 96 || info.media_mask_height != 720 ||
        info.media_mask_cell_width != 5 || info.media_mask_cell_height != 5) {
        std::cerr << "canonical Level-1 Media Mask geometry mismatch: id="
                  << info.media_mask_id.str()
                  << " mask=" << info.media_mask_width << 'x' << info.media_mask_height
                  << " cell=" << info.media_mask_cell_width << 'x'
                  << info.media_mask_cell_height << '\n';
        return 20;
    }
    std::cout << "Deimos original-data frame PASS\n"
              << "  level: " << info.level_name << " [" << info.level_id.str() << "]\n"
              << "  background: " << info.background_id.str() << '\n'
              << "  media mask: " << info.media_mask_id.str() << ' '
              << info.media_mask_width << 'x' << info.media_mask_height
              << " cell=" << info.media_mask_cell_width << 'x' << info.media_mask_cell_height << '\n'
              << "  player: " << info.player_name << " face=" << info.player_face.str()
              << " frame=" << info.player_frame << '\n'
              << "  loaded sprite groups: " << info.loaded_sprite_groups << '\n'
              << "  FPS max rate: " << info.fps_max_rate << '\n'
              << "  display: " << frame.width << 'x' << frame.height << "x16\n"
              << "  terrain copied: " << (result.world.terrain_viewport_copied ? "yes" : "no") << '\n'
              << "  score bar P1 rasterized: " << (result.score_bar_rasterized[0] ? "yes" : "no") << '\n'
              << "  frame FNV64: 0x" << std::hex << initial_hash << std::dec << '\n';

    const auto tick1 = preview->tick();
    if (!preview->render(frame, &result, &error)) {
        std::cerr << "original frame tick-1 render failed: " << error << '\n';
        return 6;
    }
    const auto tick1_hash = fnv1a64(frame.pixels);

    deimos::OriginalGameFrameTickResult tick30 = tick1;
    for (int i = 1; i < 30; ++i) tick30 = preview->tick();
    if (!preview->render(frame, &result, &error)) {
        std::cerr << "original frame tick-30 render failed: " << error << '\n';
        return 7;
    }
    const auto tick30_hash = fnv1a64(frame.pixels);
    if (tick1_hash != deimos::kCanonicalOriginalGameTick1FrameFnv64 ||
        tick30_hash != deimos::kCanonicalOriginalGameTick30FrameFnv64) {
        std::cerr << "live frame oracle mismatch: tick1=0x" << std::hex << tick1_hash
                  << " expected=0x" << deimos::kCanonicalOriginalGameTick1FrameFnv64
                  << " tick30=0x" << tick30_hash
                  << " expected=0x" << deimos::kCanonicalOriginalGameTick30FrameFnv64
                  << std::dec << '\n';
        return 8;
    }
    std::cout << "  tick 1: sourceTop=" << tick1.terrain_source_top
              << " delta=" << tick1.terrain_applied_vertical_delta
              << " FNV64=0x" << std::hex << tick1_hash << std::dec << '\n'
              << "  tick 30: sourceTop=" << tick30.terrain_source_top
              << " delta=" << tick30.terrain_applied_vertical_delta
              << " FNV64=0x" << std::hex << tick30_hash << std::dec << '\n';

    auto controlled = deimos::OriginalGameFramePreview::load(
        std::filesystem::path(argv[1]), {{'l','e','0','1'}}, 0, &error);
    if (!controlled || !controlled->render(frame, &result, &error)) {
        std::cerr << "controlled preview setup failed: " << error << '\n';
        return 9;
    }
    deimos::PreviewPlayerControlInput right;
    right.right = true;
    const auto controlled_tick = controlled->tick(right);
    if (!controlled->render(frame, &result, &error)) {
        std::cerr << "controlled preview render failed: " << error << '\n';
        return 10;
    }
    const auto controlled_hash = fnv1a64(frame.pixels);
    if (controlled_hash != deimos::kCanonicalOriginalGameRightTick1FrameFnv64) {
        std::cerr << "controlled frame oracle mismatch: got 0x" << std::hex << controlled_hash
                  << " expected=0x" << deimos::kCanonicalOriginalGameRightTick1FrameFnv64
                  << std::dec << '\n';
        return 11;
    }
    std::cout << "  control right tick 1: player=(" << controlled->player_runtime().x
              << ',' << controlled->player_runtime().y << ") velocity=("
              << controlled->player_runtime().velocity_x << ','
              << controlled->player_runtime().velocity_y << ") sourceTop="
              << controlled_tick.terrain_source_top
              << " FNV64=0x" << std::hex << controlled_hash << std::dec << '\n';

    auto world = deimos::OriginalGameFramePreview::load(
        std::filesystem::path(argv[1]), {{'l','e','0','1'}}, 0, &error);
    if (!world || !world->enable_live_world(&error)) {
        std::cerr << "live-world bootstrap failed: " << error << '\n';
        return 12;
    }
    if (!world->render(frame, &result, &error)) {
        std::cerr << "live-world initial render failed: " << error << '\n';
        return 13;
    }
    const auto world_initial_hash = fnv1a64(frame.pixels);
    const auto world_initial_members = world->entity_world().members().size();
    const auto world_initial_active = world->entity_world().active_member_count();
    const auto world_initial_placements = world->activated_level_placements();
    deimos::OriginalGameLiveInput live_input;
    live_input.weapons.fire_air = true;
    const auto live_tick = world->tick_live(live_input, &error);
    if (!error.empty() || !world->render(frame, &result, &error)) {
        std::cerr << "live-world weapon tick failed: " << error << '\n';
        return 14;
    }
    const auto world_fire_hash = fnv1a64(frame.pixels);
    std::size_t max_active = live_tick.active_entities;
    std::size_t collision_total = live_tick.collisions;
    std::size_t collision_spawn_total = live_tick.collision_spawns_due;
    std::size_t removal_total = live_tick.removals;
    std::size_t removal_consequence_total = live_tick.removal_consequences;
    std::size_t removal_spawn_total = live_tick.removal_spawns;
    std::size_t player_effect_spawn_total = live_tick.player_effect_spawns;
    std::size_t far_offscreen_cull_total = live_tick.far_offscreen_culled;
    std::size_t pruned_member_total = live_tick.pruned_members;
    std::size_t max_active_particles = live_tick.active_particles;
    std::uint64_t first_level_activation_tick = live_tick.level_placements_activated ? live_tick.frame.tick_index : 0;
    deimos::OriginalGameLiveTickResult soak_tick = live_tick;
    for (int i = 1; i < 120; ++i) {
        soak_tick = world->tick_live({}, &error);
        if (!error.empty()) {
            std::cerr << "live-world soak tick failed: " << error << '\n';
            return 15;
        }
        max_active = std::max(max_active, soak_tick.active_entities);
        collision_total += soak_tick.collisions;
        collision_spawn_total += soak_tick.collision_spawns_due;
        removal_total += soak_tick.removals;
        removal_consequence_total += soak_tick.removal_consequences;
        removal_spawn_total += soak_tick.removal_spawns;
        player_effect_spawn_total += soak_tick.player_effect_spawns;
        far_offscreen_cull_total += soak_tick.far_offscreen_culled;
        pruned_member_total += soak_tick.pruned_members;
        max_active_particles = std::max(max_active_particles, soak_tick.active_particles);
        if (first_level_activation_tick == 0 && soak_tick.level_placements_activated != 0) {
            first_level_activation_tick = soak_tick.frame.tick_index;
        }
    }
    if (!world->render(frame, &result, &error)) {
        std::cerr << "live-world soak render failed: " << error << '\n';
        return 16;
    }
    const auto world_soak_hash = fnv1a64(frame.pixels);
    if (world_initial_members != 2 || world_initial_active != 2 ||
        world_initial_placements != 2 || world->activated_level_placements() != 3 ||
        first_level_activation_tick != 36 ||
        world->entity_world().members().size() != 15 ||
        world->entity_world().groups().size() != 9 ||
        collision_total != 0 || collision_spawn_total != 0 || removal_total != 9 ||
        removal_consequence_total != 12 || removal_spawn_total != 1 ||
        player_effect_spawn_total != 1 || far_offscreen_cull_total != 2 ||
        pruned_member_total != 9 || max_active_particles != 25 ||
        max_active != 18 ||
        world_initial_hash != deimos::kLiveWorldIntegrationInitialFrameFnv64 ||
        world_fire_hash != deimos::kLiveWorldIntegrationFireTick1FrameFnv64 ||
        world_soak_hash != deimos::kLiveWorldIntegrationTick120FrameFnv64) {
        std::cerr << "live-world integration oracle mismatch: members=" << world_initial_members
                  << " initialActive=" << world_initial_active
                  << " initialPlacements=" << world_initial_placements
                  << " placements120=" << world->activated_level_placements()
                  << " firstPlacementTick=" << first_level_activation_tick
                  << " resident120=" << world->entity_world().members().size()
                  << " groups120=" << world->entity_world().groups().size()
                  << " collisions120=" << collision_total
                  << " collisionSpawns120=" << collision_spawn_total
                  << " removals120=" << removal_total
                  << " removalConsequences120=" << removal_consequence_total
                  << " removalSpawns120=" << removal_spawn_total
                  << " playerEffectSpawns120=" << player_effect_spawn_total
                  << " farOffscreenCull120=" << far_offscreen_cull_total
                  << " prunedMembers120=" << pruned_member_total
                  << " maxParticles120=" << max_active_particles
                  << " maxActive120=" << max_active
                  << " initial=0x" << std::hex << world_initial_hash
                  << " fire=0x" << world_fire_hash
                  << " tick120=0x" << world_soak_hash << std::dec << '\n';
        return 17;
    }
    std::cout << "  live world: initialMembers=" << world_initial_members
              << " initialActive=" << world_initial_active
              << " initialPlacements=" << world_initial_placements
              << " placements120=" << world->activated_level_placements()
              << " firstPlacementTick=" << first_level_activation_tick
              << " resident120=" << world->entity_world().members().size()
              << " groups120=" << world->entity_world().groups().size()
              << " activeAfterFire=" << live_tick.active_entities
              << " firstTickGroups=" << live_tick.constructed_groups
              << " firstTickMembers=" << live_tick.constructed_members
              << " collisions120=" << collision_total
              << " collisionSpawns120=" << collision_spawn_total
              << " removals120=" << removal_total
              << " removalConsequences120=" << removal_consequence_total
              << " removalSpawns120=" << removal_spawn_total
              << " playerEffectSpawns120=" << player_effect_spawn_total
              << " farOffscreenCull120=" << far_offscreen_cull_total
              << " prunedMembers120=" << pruned_member_total
              << " maxParticles120=" << max_active_particles
              << " maxActive120=" << max_active
              << " initialFNV64=0x" << std::hex << world_initial_hash
              << " fireTickFNV64=0x" << world_fire_hash
              << " tick120FNV64=0x" << world_soak_hash << std::dec << '\n';

    auto live_controlled = deimos::OriginalGameFramePreview::load(
        std::filesystem::path(argv[1]), {{'l','e','0','1'}}, 0, &error);
    if (!live_controlled || !live_controlled->enable_live_world(&error)) {
        std::cerr << "live-controlled setup failed: " << error << '\n';
        return 18;
    }
    deimos::OriginalGameLiveInput live_right;
    live_right.movement.right = true;
    const auto live_right_tick = live_controlled->tick_live(live_right, &error);
    const auto& live_right_player = live_controlled->player_runtime();
    if (!error.empty() || !live_right_tick.frame.player_control.active ||
        live_right_player.x != 209.6f || live_right_player.y != 330.0f ||
        live_right_player.velocity_x != 1.6f || live_right_player.velocity_y != 0.0f) {
        std::cerr << "live Player-1 authority oracle mismatch: player=("
                  << live_right_player.x << ',' << live_right_player.y << ") velocity=("
                  << live_right_player.velocity_x << ',' << live_right_player.velocity_y << ")\n";
        return 19;
    }
    std::cout << "  live control right tick 1: player=(" << live_right_player.x
              << ',' << live_right_player.y << ") velocity=("
              << live_right_player.velocity_x << ',' << live_right_player.velocity_y << ")\n";

    // Ground/secondary fire must be an end-to-end playable-host fact, not just
    // a unit-test constructor result. Reuse the already-loaded live-control
    // world so this external frame probe does not pay another full PAK load.
    deimos::OriginalGameLiveInput ground_input;
    ground_input.weapons.fire_ground = true;
    const auto ground_tick = live_controlled->tick_live(ground_input, &error);
    if (!error.empty() || !ground_tick.weapons.ground_launched ||
        !live_controlled->entity_world().has_active_unit(deimos::FourCC{{'p','l','b','o'}}) ||
        !live_controlled->entity_world().has_active_unit(deimos::FourCC{{'p','b','l','f'}})) {
        std::cerr << "canonical Level-1 ground-fire oracle mismatch\n";
        return 21;
    }
    std::cout << "  secondary fire: Plasma Bomb launch PASS\n";

    return 0;
}
