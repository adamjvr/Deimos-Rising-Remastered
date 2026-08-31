#include "deimos/original_game_frame_preview.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>

namespace {
std::optional<deimos::OriginalGameFramePreview> load_world(
    const std::filesystem::path& paks,
    std::string& error) {
    auto world = deimos::OriginalGameFramePreview::load(
        paks, {{'l','e','0','1'}}, 0, &error);
    if (!world || !world->enable_live_world(&error)) return std::nullopt;
    return world;
}
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: deimos_playable_runtime_probe /path/to/Paks\n";
        return 2;
    }
    const std::filesystem::path paks = argv[1];
    std::string error;

    // Crash/lifecycle witness: hold north long enough to enter the opening
    // combat lane, then let the recovered damage/death/respawn machine run.
    auto crash = load_world(paks, error);
    if (!crash) {
        std::cerr << "playable crash setup failed: " << error << '\n';
        return 3;
    }
    std::uint64_t first_dying_tick = 0;
    std::uint64_t first_respawn_tick = 0;
    std::size_t player_effects = 0;
    std::size_t max_particles = 0;
    for (int i = 0; i < 300; ++i) {
        deimos::OriginalGameLiveInput input;
        if (i < 100) input.movement.up = true;
        const auto tick = crash->tick_live(input, &error);
        if (!error.empty()) {
            std::cerr << "playable crash tick failed: " << error << '\n';
            return 4;
        }
        player_effects += tick.player_effect_spawns;
        max_particles = std::max(max_particles, tick.active_particles);
        const auto& player = crash->player_runtime();
        if (first_dying_tick == 0 &&
            player.status == static_cast<int>(deimos::LegacyPlayerStatus::dying)) {
            first_dying_tick = tick.frame.tick_index;
        }
        if (first_dying_tick != 0 && first_respawn_tick == 0 &&
            player.status == static_cast<int>(deimos::LegacyPlayerStatus::active) &&
            player.lives == 2) {
            first_respawn_tick = tick.frame.tick_index;
        }
    }
    const auto& crash_player = crash->player_runtime();
    if (first_dying_tick != 171 || first_respawn_tick != 252 ||
        player_effects < 5 || max_particles == 0 ||
        crash_player.lives != 2 ||
        crash_player.status != static_cast<int>(deimos::LegacyPlayerStatus::active) ||
        crash_player.x != 208.0f || crash_player.y != 330.0f ||
        crash_player.shield_percentage != 100.0f) {
        std::cerr << "crash/respawn oracle mismatch: dying=" << first_dying_tick
                  << " respawn=" << first_respawn_tick
                  << " effects=" << player_effects
                  << " maxParticles=" << max_particles
                  << " lives=" << crash_player.lives
                  << " status=" << crash_player.status
                  << " pos=(" << crash_player.x << ',' << crash_player.y << ')'
                  << " shield=" << crash_player.shield_percentage << '\n';
        return 5;
    }

    // Ground-fire witness: this must prove actual air-to-ground damage, not
    // merely that the host weapon bridge accepted a launch. Move left through
    // the opening lane, fire one canonical Plasma Bomb, and require the left
    // Bonus Station (`bsde`) to lose exactly one 0.4-shield hit when the bomb
    // enters its colliding ground phase. This reproduces the player-facing
    // secondary-fire complaint directly against original Level-1 data.
    auto ground = load_world(paks, error);
    if (!ground) {
        std::cerr << "playable ground-fire setup failed: " << error << '\n';
        return 6;
    }
    float ground_target_before = -1.0f;
    float ground_target_after = -1.0f;
    bool ground_damage_launch_seen = false;
    bool ground_crosshair_seen = false;
    bool ground_crosshair_locked_seen = false;
    bool ground_crosshair_position_ok = true;
    for (const auto& entity : ground->entity_world().members()) {
        if (entity.unit_id == deimos::FourCC{{'b','s','d','e'}} && entity.x < 100.0f) {
            ground_target_before = entity.shields;
            break;
        }
    }
    for (int i = 0; i < 50; ++i) {
        deimos::OriginalGameLiveInput input;
        if (i < 30) input.movement.left = true;
        if (i == 18) input.weapons.fire_ground = true;
        const auto tick = ground->tick_live(input, &error);
        if (!error.empty()) {
            std::cerr << "playable ground-fire tick failed: " << error << '\n';
            return 7;
        }
        ground_damage_launch_seen = ground_damage_launch_seen || tick.weapons.ground_launched;

        const auto& player = ground->player_runtime();
        if (tick.ground_crosshair_enabled &&
            tick.ground_crosshair_face == deimos::FourCC{{'p','b','t','a'}}) {
            ground_crosshair_seen = true;
            if (std::abs(tick.ground_crosshair_x - player.x) > 0.001f ||
                std::abs(tick.ground_crosshair_y - (player.y - 121.0f)) > 0.001f) {
                ground_crosshair_position_ok = false;
            }
            if (tick.ground_crosshair_locked && tick.ground_crosshair_frame == 1) {
                ground_crosshair_locked_seen = true;
            }
        }
    }
    for (const auto& entity : ground->entity_world().members()) {
        if (entity.unit_id == deimos::FourCC{{'b','s','d','e'}} && entity.x < 100.0f) {
            ground_target_after = entity.shields;
            break;
        }
    }
    if (!ground_damage_launch_seen || !ground_crosshair_seen ||
        !ground_crosshair_locked_seen || !ground_crosshair_position_ok ||
        ground_target_before != 4.0f || ground_target_after != 3.6f) {
        std::cerr << "ground-fire damage/reticle oracle mismatch: launched="
                  << ground_damage_launch_seen
                  << " reticle=" << ground_crosshair_seen
                  << " locked=" << ground_crosshair_locked_seen
                  << " position=" << ground_crosshair_position_ok
                  << " shield=" << ground_target_before << " -> " << ground_target_after << '\n';
        return 8;
    }

    // Hold-to-charge witness from the original Level-1 Ion Cannon fields.
    // A press still emits the ordinary shot, charging activates after 15
    // ticks, the meter climbs in 5%% steps (20 levels), and release begins the
    // canonical icps release-spawner stream.
    auto charge = load_world(paks, error);
    if (!charge) {
        std::cerr << "playable charge setup failed: " << error << '\n';
        return 9;
    }
    bool charge_activation_seen = false;
    float max_charge_percentage = 0.0f;
    for (int i = 0; i < 25; ++i) {
        deimos::OriginalGameLiveInput input;
        input.weapons.fire_air = true;
        const auto tick = charge->tick_live(input, &error);
        if (!error.empty()) {
            std::cerr << "playable charge tick failed: " << error << '\n';
            return 10;
        }
        charge_activation_seen = charge_activation_seen || tick.weapons.air_powerup_activated;
        max_charge_percentage = std::max(max_charge_percentage, tick.weapons.air_power_percentage);
    }
    const auto charge_release = charge->tick_live({}, &error);
    if (!error.empty()) {
        std::cerr << "playable charge release failed: " << error << '\n';
        return 11;
    }
    bool release_spawner_seen = false;
    for (const auto& request : charge_release.weapons.powerup_requests) {
        release_spawner_seen = release_spawner_seen ||
            request.unit_id == deimos::FourCC{{'i','c','p','s'}};
    }
    if (!charge_activation_seen || max_charge_percentage < 20.0f ||
        !charge_release.weapons.air_powerup_released || !release_spawner_seen) {
        std::cerr << "charge oracle mismatch: activation=" << charge_activation_seen
                  << " maxPower=" << max_charge_percentage
                  << " released=" << charge_release.weapons.air_powerup_released
                  << " releaseSpawner=" << release_spawner_seen << '\n';
        return 12;
    }

    // Long-run playable stress: continuously request primary fire and drop a
    // ground weapon every second. This catches both dead-history retention and
    // far-offscreen live-list leaks without coupling correctness to wall time.
    auto stress = load_world(paks, error);
    if (!stress) {
        std::cerr << "playable stress setup failed: " << error << '\n';
        return 13;
    }
    std::size_t max_resident = stress->entity_world().members().size();
    std::size_t max_active = stress->entity_world().active_member_count();
    std::size_t total_pruned = 0;
    std::size_t total_far_culled = 0;
    bool ground_launch_seen = false;
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 3000; ++i) {
        deimos::OriginalGameLiveInput input;
        input.weapons.fire_air = true;
        if ((i % 30) == 0) input.weapons.fire_ground = true;
        const auto tick = stress->tick_live(input, &error);
        if (!error.empty()) {
            std::cerr << "playable stress tick failed: " << error << '\n';
            return 14;
        }
        ground_launch_seen = ground_launch_seen || tick.weapons.ground_launched;
        total_pruned += tick.pruned_members;
        total_far_culled += tick.far_offscreen_culled;
        max_resident = std::max(max_resident, stress->entity_world().members().size());
        max_active = std::max(max_active, tick.active_entities);
    }
    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    const auto final_resident = stress->entity_world().members().size();

    if (!ground_launch_seen) {
        std::cerr << "secondary-fire launch was never accepted\n";
        return 15;
    }
    if (max_resident > 128 || final_resident > 32 ||
        total_pruned < 1000 || total_far_culled == 0) {
        std::cerr << "long-run bounded-world oracle mismatch: maxResident=" << max_resident
                  << " finalResident=" << final_resident
                  << " maxActive=" << max_active
                  << " pruned=" << total_pruned
                  << " farCulled=" << total_far_culled << '\n';
        return 16;
    }

    std::cout << "Deimos playable-runtime PASS\n"
              << "  crash: dying@" << first_dying_tick
              << " respawn@" << first_respawn_tick
              << " lives=" << crash_player.lives
              << " playerEffects=" << player_effects
              << " maxParticles=" << max_particles << '\n'
              << "  secondary fire: Plasma Bomb hit bsde " << ground_target_before
              << " -> " << ground_target_after
              << " reticle=pbta normal/locked offset=(0,-121)\n"
              << "  charge: activation=15 ticks maxObserved=" << max_charge_percentage
              << "% release=icps\n"
              << "  stress3000: maxResident=" << max_resident
              << " finalResident=" << final_resident
              << " maxActive=" << max_active
              << " pruned=" << total_pruned
              << " farCulled=" << total_far_culled
              << " elapsedMs=" << elapsed_ms << '\n';
    return 0;
}
