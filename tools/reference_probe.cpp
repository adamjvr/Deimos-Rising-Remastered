#include "deimos/audio_resource.hpp"
#include "deimos/collision_runtime.hpp"
#include "deimos/data_tables.hpp"
#include "deimos/destruction_runtime.hpp"
#include "deimos/entity_runtime.hpp"
#include "deimos/entity_world.hpp"
#include "deimos/film.hpp"
#include "deimos/game_definitions.hpp"
#include "deimos/legacy_text.hpp"
#include "deimos/level.hpp"
#include "deimos/pak_archive.hpp"
#include "deimos/player_definition.hpp"
#include "deimos/player_runtime.hpp"
#include "deimos/render_runtime.hpp"
#include "deimos/render_backend.hpp"
#include "deimos/sprite_resource.hpp"
#include "deimos/terrain_runtime.hpp"
#include "deimos/unit_definition.hpp"
#include "deimos/unit_behavior.hpp"
#include "deimos/weapon_definition.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <cctype>
#include <filesystem>
#include <span>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {
std::uint64_t fnv1a64_bytes(std::uint64_t hash, std::span<const std::uint8_t> bytes) {
    constexpr std::uint64_t prime = 1099511628211ull;
    for (const auto byte : bytes) {
        hash ^= byte;
        hash *= prime;
    }
    return hash;
}

std::uint64_t fnv1a64_u16(std::uint64_t hash, std::uint16_t value) {
    const std::array<std::uint8_t, 2> bytes{{
        static_cast<std::uint8_t>(value >> 8u),
        static_cast<std::uint8_t>(value),
    }};
    return fnv1a64_bytes(hash, bytes);
}

std::uint64_t fnv1a64_u32(std::uint64_t hash, std::uint32_t value) {
    const std::array<std::uint8_t, 4> bytes{{
        static_cast<std::uint8_t>(value >> 24u),
        static_cast<std::uint8_t>(value >> 16u),
        static_cast<std::uint8_t>(value >> 8u),
        static_cast<std::uint8_t>(value),
    }};
    return fnv1a64_bytes(hash, bytes);
}

std::uint32_t pcm_crc32_le(std::span<const std::int16_t> samples) {
    std::uint32_t crc = 0xffffffffu;
    const auto update = [&crc](std::uint8_t byte) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1u) ^ (0xedb88320u & (0u - (crc & 1u)));
        }
    };
    for (const auto sample : samples) {
        const auto value = static_cast<std::uint16_t>(sample);
        update(static_cast<std::uint8_t>(value));
        update(static_cast<std::uint8_t>(value >> 8u));
    }
    return crc ^ 0xffffffffu;
}
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: deimos_reference_probe /path/to/Game.pak\n";
        return 2;
    }

    std::string error;
    auto pak = deimos::PakArchive::open(std::filesystem::path(argv[1]), &error);
    if (!pak) {
        std::cerr << error << '\n';
        return 3;
    }

    struct MusicOracle {
        const char* tag;
        std::uint32_t packet_groups;
        std::uint64_t decoded_frames;
        std::uint32_t pcm_crc32_le;
    };
    constexpr std::array<MusicOracle, 3> music_oracles = {{
        {"mu03", 134892u, 8633088u, 0x4f945e4eu},
        {"ammu",  23966u, 1533824u, 0x9871dd60u},
        {"inmu",  41153u, 2633792u, 0x60d31157u},
    }};
    std::size_t music_resources_validated = 0;
    std::size_t audio_resources_validated = 0;
    std::uint64_t audio_frames_validated = 0;
    const auto game_pak_path = std::filesystem::path(argv[1]);
    const auto music_pak_path = game_pak_path.parent_path() / "Music.pak";
    const auto audio_pak_path = game_pak_path.parent_path() / "Audio.pak";
    if (std::filesystem::exists(audio_pak_path)) {
        auto audio_pak = deimos::PakArchive::open(audio_pak_path, &error);
        if (!audio_pak) {
            std::cerr << "Audio.pak: " << error << '\n';
            return 28;
        }
        for (const auto& entry : audio_pak->entries()) {
            if (entry.is_directory) continue;
            const auto name = deimos::parse_resource_name(entry.path);
            if (!name || name->kind != deimos::ResourceKind::audio) {
                std::cerr << "Audio.pak: unexpected non-audio resource " << entry.path << '\n';
                return 29;
            }
            auto bytes = audio_pak->read(entry, &error);
            if (!bytes) {
                std::cerr << entry.path << ": " << error << '\n';
                return 30;
            }
            const auto pcm = deimos::decode_legacy_ima4_aifc(*bytes, &error);
            if (!pcm || pcm->channels != 1 || pcm->sample_rate != 44100) {
                std::cerr << entry.path << ": canonical SFX IMA4 mismatch: " << error << '\n';
                return 31;
            }
            ++audio_resources_validated;
            audio_frames_validated += pcm->frame_count();
        }
        if (audio_resources_validated != 96 || audio_frames_validated != 3133376u) {
            std::cerr << "Audio.pak: canonical SFX corpus count/frame mismatch\n";
            return 32;
        }
    }

    if (std::filesystem::exists(music_pak_path)) {
        auto music_pak = deimos::PakArchive::open(music_pak_path, &error);
        if (!music_pak) {
            std::cerr << "Music.pak: " << error << '\n';
            return 22;
        }
        for (const auto& entry : music_pak->entries()) {
            if (entry.is_directory) continue;
            const auto name = deimos::parse_resource_name(entry.path);
            if (!name || name->kind != deimos::ResourceKind::audio) continue;
            const auto tag = name->tag.str();
            const auto oracle = std::find_if(music_oracles.begin(), music_oracles.end(),
                [&](const MusicOracle& value) { return tag == value.tag; });
            if (oracle == music_oracles.end()) {
                std::cerr << "Music.pak: unexpected audio resource " << entry.path << '\n';
                return 23;
            }
            auto bytes = music_pak->read(entry, &error);
            if (!bytes) {
                std::cerr << entry.path << ": " << error << '\n';
                return 24;
            }
            const auto info = deimos::parse_legacy_aifc(*bytes, &error);
            if (!info || info->channels != 2 || info->sample_size_bits != 16 ||
                info->sample_rate != 44100.0 || info->compression.str() != "ima4" ||
                info->packet_groups != oracle->packet_groups ||
                info->decoded_frame_count() != oracle->decoded_frames) {
                std::cerr << entry.path << ": canonical AIFC/ima4 metadata mismatch: " << error << '\n';
                return 25;
            }
            const auto pcm = deimos::decode_legacy_ima4_aifc(*bytes, &error);
            if (!pcm || pcm->channels != 2 || pcm->sample_rate != 44100 ||
                pcm->frame_count() != oracle->decoded_frames ||
                pcm_crc32_le(pcm->interleaved_samples) != oracle->pcm_crc32_le) {
                std::cerr << entry.path << ": canonical IMA4 PCM mismatch: " << error << '\n';
                return 26;
            }
            ++music_resources_validated;
        }
        if (music_resources_validated != music_oracles.size()) {
            std::cerr << "Music.pak: expected exactly three canonical audio resources\n";
            return 27;
        }
    }

    std::size_t files = 0, levels = 0, objects = 0, films = 0;
    std::size_t id_lists = 0, float_lists = 0, color_lists = 0;
    std::size_t text_formats = 0, string_lists = 0, rect_lists = 0;
    std::size_t units = 0, unit_states = 0, unit_spawn_sets = 0, unit_rules = 0;
    std::size_t spawn_repeat = 0, spawn_absolute = 0, spawn_rotated_offset = 0;
    std::size_t spawn_offscreen_guard = 0, spawn_while_fleeing = 0, spawn_set_heading = 0;
    std::size_t spawn_pause_rotation = 0, spawn_terrain_effects = 0, spawn_reversed_ranges = 0;
    std::size_t unit_terrain_effects = 0, unit_adjust_owner_scale = 0, unit_player_active_only = 0;
    std::size_t state_lock_owner = 0, state_link_owner = 0, state_orbit_owner = 0;
    std::size_t state_hunt = 0, state_hold = 0, state_cyclic = 0;
    std::size_t state_delete_no_player = 0, state_destruct_no_player = 0;
    std::size_t state_collides = 0, state_pass_hits_owner = 0;
    std::size_t state_collision_invulnerable = 0, state_collides_players = 0;
    std::size_t state_no_collision_glow = 0, state_collision_spawns = 0;
    std::size_t state_shield_depletion = 0, unit_shield_depletion_state = 0;
    std::size_t unit_ground_collision_domain = 0, unit_air_collision_domain = 0;
    std::size_t unit_harmless = 0, unit_player_projectile = 0, unit_player_projectile_hittable = 0;
    std::size_t unit_nonzero_collision_damage = 0, unit_nonzero_shields = 0;
    std::size_t unit_pickups = 0;
    std::size_t pickup_coin = 0, pickup_multiplier = 0, pickup_extra_life = 0, pickup_shield = 0;
    std::size_t pickup_air = 0, pickup_ground = 0, pickup_special = 0, pickup_other = 0;
    std::size_t unit_casts_shadows = 0, unit_ground_obstacle_collision = 0;
    std::size_t unit_adjust_shadow_scaling = 0, unit_scale_tolerance = 0;
    std::size_t state_draw_to_terrain_visual = 0, state_do_colorise = 0;
    std::size_t state_nondefault_visibility = 0, state_nondefault_scale = 0, state_nonzero_tint = 0;
    std::size_t layer_defa = 0, layer_grou = 0, layer_grhi = 0, layer_ailo = 0, layer_aihi = 0;
    std::size_t layer_plwe = 0, layer_play = 0, layer_plsh = 0, layer_plef = 0, layer_plui = 0;
    std::size_t layer_atmo = 0, layer_hud = 0, layer_none = 0, layer_other = 0;
    std::size_t unit_death_spawn_any_media = 0, unit_media_impact_size = 0;
    std::size_t unit_destruction_spawns = 0, unit_deletion_spawns = 0;
    std::size_t unit_destruction_particles = 0, unit_destruction_notices = 0;
    std::size_t unit_destruction_sounds = 0, unit_destruction_coin_rewards = 0;
    std::size_t unit_group_kill_coin_rewards = 0, unit_destroy_children = 0;
    std::size_t unit_delete_children = 0, unit_create_obstacle = 0;
    std::size_t unit_draw_to_terrain = 0, unit_random_bonus = 0;
    std::size_t state_destroy_with_owner = 0, state_delete_with_owner = 0;
    std::size_t state_destroy_owner = 0;
    std::size_t units_with_lock_owner = 0, units_with_link_owner = 0, units_with_orbit_owner = 0;
    std::size_t sprite_alpha_plates = 0, sprite_color_plates = 0, sprite_frames = 0;
    std::size_t sprite_plate_pairs = 0;
    std::size_t pl1b_frames = 0, exlg_frames = 0, bocr_frames = 0, glow_frames = 0;
    std::pair<int, int> pl1b_frame0{};
    std::map<std::string, std::pair<int, int>> sprite_alpha_dimensions;
    std::map<std::string, std::pair<int, int>> sprite_color_dimensions;
    std::map<std::string, std::pair<deimos::FourCC, deimos::LegacyIndexedImage>> sprite_alpha_images;
    std::map<std::string, std::pair<deimos::FourCC, deimos::LegacyIndexedImage>> sprite_color_images;
    std::size_t weapons = 0, weapon_spawns = 0, players = 0;
    std::vector<std::pair<std::string, deimos::CompiledPlayerRuntimeDefinition>> player_runtime_defs;
    std::size_t unresolved_active_actions = 0, unresolved_inert_actions = 0, unknown_rule_conditions = 0;

    std::optional<deimos::NamedTable<float>> canonical_game_floats;
    std::optional<deimos::NamedTable<deimos::FourCC>> canonical_game_objects;

    for (const auto& entry : pak->entries()) {
        if (entry.is_directory) continue;
        ++files;
        auto bytes = pak->read(entry, &error); // also CRC-validates
        if (!bytes) {
            std::cerr << entry.path << ": " << error << '\n';
            return 4;
        }

        const auto dot = entry.path.find_last_of('.');
        if (dot == std::string::npos) continue;
        const auto ext = entry.path.substr(dot);

        if (ext == ".gif") {
            const auto resource_name = deimos::parse_resource_name(entry.path);
            if (resource_name && resource_name->plate != deimos::PlateKind::none) {
                auto decoded = deimos::decode_legacy_gif_indices(*bytes, &error);
                if (!decoded) {
                    std::cerr << entry.path << ": GIF index decode failed: " << error << '\n';
                    return 20;
                }
                const auto dimensions = std::pair<int, int>{decoded->width, decoded->height};
                if (resource_name->plate == deimos::PlateKind::alpha) {
                    ++sprite_alpha_plates;
                    sprite_alpha_dimensions[resource_name->display_name] = dimensions;
                    auto frames = deimos::extract_legacy_sprite_frames(*decoded, &error);
                    if (!frames) {
                        std::cerr << entry.path << ": legacy sprite-frame scan failed: " << error << '\n';
                        return 21;
                    }
                    sprite_frames += frames->size();
                    const auto tag = resource_name->tag.str();
                    if (tag == "PL1B") {
                        pl1b_frames = frames->size();
                        if (!frames->empty()) pl1b_frame0 = {(*frames)[0].width, (*frames)[0].height};
                    } else if (tag == "EXLG") exlg_frames = frames->size();
                    else if (tag == "BOCR") bocr_frames = frames->size();
                    else if (tag == "GLOW") glow_frames = frames->size();
                    sprite_alpha_images[resource_name->display_name] = {resource_name->tag, std::move(*decoded)};
                } else {
                    ++sprite_color_plates;
                    sprite_color_dimensions[resource_name->display_name] = dimensions;
                    sprite_color_images[resource_name->display_name] = {resource_name->tag, std::move(*decoded)};
                }
            }
        }

        if (ext == ".leve") {
            auto level = deimos::decode_and_parse_level(*bytes, &error);
            if (!level) { std::cerr << entry.path << ": " << error << '\n'; return 5; }
            ++levels;
            objects += level->objects.size();
        } else if (ext == ".film") {
            if (!deimos::parse_film_v10005(*bytes, &error)) {
                std::cerr << entry.path << ": " << error << '\n'; return 6;
            }
            ++films;
        } else if (ext == ".unde") {
            auto unit = deimos::decode_and_parse_unit_definition(*bytes, &error);
            if (!unit) { std::cerr << entry.path << ": " << error << '\n'; return 9; }
            ++units;
            unit_terrain_effects += unit->core_fields.bool_value("terrainEffect_BOOL").value_or(false);
            unit_adjust_owner_scale += unit->core_fields.bool_value("adjustInitialLocForOwnerScale_BOOL").value_or(false);
            unit_player_active_only += unit->core_fields.bool_value("canBeSpawnedOnlyWhenPlayersActive_BOOL").value_or(false);
            unit_states += unit->states.size();
            const auto behavior = deimos::compile_unit_behavior(*unit);
            const bool ground = unit->core_fields.bool_value("isGroundBased_BOOL").value_or(false);
            const deimos::FourCC expected_domain = ground
                ? deimos::FourCC{{'g','r','n','d'}}
                : deimos::FourCC{{'a','i','r',' '}};
            if (behavior.initial_scale_percent != unit->core_fields.int_value("initialScalePercent_INT").value_or(0) ||
                behavior.initial_scale_tolerance_percent != unit->core_fields.int_value("initialScalePercentTolerance_INT").value_or(0) ||
                behavior.initial_visibility_percent != unit->core_fields.int_value("initialVisibilityPercent_INT").value_or(0) ||
                !(behavior.draw_layer == unit->core_fields.id_value("drawLayer_ID").value_or(deimos::FourCC{})) ||
                behavior.adjust_shadow_location_for_scaling != unit->core_fields.bool_value("adjustShadowLocForScaling_BOOL").value_or(false) ||
                !(behavior.collision_domain == expected_domain) ||
                behavior.harmless_to_players != unit->core_fields.bool_value("harmlessToPlayers_BOOL").value_or(false) ||
                behavior.player_projectile != unit->core_fields.bool_value("playerProjectile_BOOL").value_or(false) ||
                behavior.can_be_hit_by_player_projectile != unit->core_fields.bool_value("canBeHitByPlayerProjectile_BOOL").value_or(false) ||
                behavior.collision_damage != unit->core_fields.float_value("damage_FLOAT").value_or(0.0f) ||
                behavior.shields_base != unit->core_fields.float_value("shields_BaseAmount_FLOAT").value_or(0.0f) ||
                behavior.casts_shadows != unit->core_fields.bool_value("castsShadows_BOOL").value_or(false) ||
                behavior.collides_with_ground_obstacles != unit->core_fields.bool_value("collidesWithGroundObstacles_BOOL").value_or(false) ||
                behavior.death_spawn_on_any_media != unit->core_fields.bool_value("doDeathSpawnOnAnyMedia_BOOL").value_or(false) ||
                !(behavior.media_impact_size == unit->core_fields.id_value("mediaImpactSize_ID").value_or(deimos::FourCC{})) ||
                behavior.score != unit->core_fields.int_value("score_INT").value_or(0) ||
                !(behavior.deletion_spawn == unit->core_fields.id_value("deletionSpawn_ID").value_or(deimos::FourCC{})) ||
                !(behavior.destruction_spawn == unit->core_fields.id_value("destructSpawn_ID").value_or(deimos::FourCC{})) ||
                !(behavior.destruction_particles == unit->core_fields.id_value("destructParticle_ID").value_or(deimos::FourCC{})) ||
                !(behavior.destruction_particle_color == unit->core_fields.color_value("destructParticleColor_COLOR").value_or(deimos::Rgb24{})) ||
                behavior.destruction_notice != unit->core_fields.string_value("destructNotice_STR").value_or(std::string_view{}) ||
                behavior.destruction_coin_count != unit->core_fields.int_value("destructNumCoinsToRelease_INT").value_or(0) ||
                !(behavior.destruction_coin == unit->core_fields.id_value("destructCoin_ID").value_or(deimos::FourCC{})) ||
                !(behavior.destruction_group_kill_coin == unit->core_fields.id_value("destructCoinOnGroupKill_ID").value_or(deimos::FourCC{})) ||
                behavior.destruction_destroy_children != unit->core_fields.bool_value("destructDestroyChildren_BOOL").value_or(false) ||
                behavior.destruction_delete_children != unit->core_fields.bool_value("destructDeleteChildren_BOOL").value_or(false) ||
                behavior.destruction_create_obstacle != unit->core_fields.bool_value("destructCreateObstacle_BOOL").value_or(false) ||
                behavior.destruction_draw_to_terrain != unit->core_fields.bool_value("destructDrawToTerrain_BOOL").value_or(false) ||
                behavior.destruction_release_random_bonus != unit->core_fields.bool_value("destructReleaseRandomBonus_BOOL").value_or(false) ||
                !(behavior.destruction_sound.id == unit->core_fields.id_value("destructSound_ID").value_or(deimos::FourCC{})) ||
                behavior.destruction_sound.min_volume != unit->core_fields.int_value("destructSound_MinVolume_INT").value_or(0) ||
                behavior.destruction_sound.max_volume != unit->core_fields.int_value("destructSound_MaxVolume_INT").value_or(0) ||
                behavior.destruction_sound.priority != unit->core_fields.int_value("destructSound_Priority_INT").value_or(0) ||
                behavior.destruction_sound.min_pitch != unit->core_fields.float_value("destructSound_MinPitch_FLOAT").value_or(0.0f) ||
                behavior.destruction_sound.max_pitch != unit->core_fields.float_value("destructSound_MaxPitch_FLOAT").value_or(0.0f) ||
                !(behavior.pickup_type == unit->core_fields.id_value("pickup_Type_ID").value_or(deimos::FourCC{})) ||
                behavior.pickup_value != unit->core_fields.int_value("pickup_Value_INT").value_or(0)) {
                std::cerr << entry.path << ": compiled visual/collision/destruction UnitDef fields disagree with parsed source\n";
                return 19;
            }
            unit_ground_collision_domain += ground;
            unit_air_collision_domain += !ground;
            unit_harmless += behavior.harmless_to_players;
            unit_player_projectile += behavior.player_projectile;
            unit_player_projectile_hittable += behavior.can_be_hit_by_player_projectile;
            unit_nonzero_collision_damage += behavior.collision_damage != 0.0f;
            unit_nonzero_shields += behavior.shields_base != 0.0f;
            const bool is_pickup = behavior.pickup_type.str() != "none" && !(behavior.pickup_type == deimos::FourCC{});
            unit_pickups += is_pickup;
            if (is_pickup) {
                const auto pickup_type = behavior.pickup_type.str();
                if (pickup_type == "coin") ++pickup_coin;
                else if (pickup_type == "mult") ++pickup_multiplier;
                else if (pickup_type == "exli") ++pickup_extra_life;
                else if (pickup_type == "shie") ++pickup_shield;
                else if (pickup_type == "air ") ++pickup_air;
                else if (pickup_type == "grnd") ++pickup_ground;
                else if (pickup_type == "spec") ++pickup_special;
                else ++pickup_other;
            }
            const auto present = [](deimos::FourCC value) {
                return !(value == deimos::FourCC{}) && value.str() != "none" && value.str() != "NULL";
            };
            unit_casts_shadows += behavior.casts_shadows;
            unit_adjust_shadow_scaling += behavior.adjust_shadow_location_for_scaling;
            unit_scale_tolerance += behavior.initial_scale_tolerance_percent != 0;
            const auto layer = behavior.draw_layer.str();
            if (layer == "defa") ++layer_defa;
            else if (layer == "grou") ++layer_grou;
            else if (layer == "grhi") ++layer_grhi;
            else if (layer == "ailo") ++layer_ailo;
            else if (layer == "aihi") ++layer_aihi;
            else if (layer == "plwe") ++layer_plwe;
            else if (layer == "play") ++layer_play;
            else if (layer == "plsh") ++layer_plsh;
            else if (layer == "plef") ++layer_plef;
            else if (layer == "plui") ++layer_plui;
            else if (layer == "atmo") ++layer_atmo;
            else if (layer == "hud ") ++layer_hud;
            else if (layer == "none" || behavior.draw_layer == deimos::FourCC{}) ++layer_none;
            else ++layer_other;
            unit_ground_obstacle_collision += behavior.collides_with_ground_obstacles;
            unit_death_spawn_any_media += behavior.death_spawn_on_any_media;
            unit_media_impact_size += present(behavior.media_impact_size);
            unit_destruction_spawns += present(behavior.destruction_spawn);
            unit_deletion_spawns += present(behavior.deletion_spawn);
            unit_destruction_particles += present(behavior.destruction_particles);
            unit_destruction_notices += !behavior.destruction_notice.empty();
            unit_destruction_sounds += present(behavior.destruction_sound.id);
            unit_destruction_coin_rewards += behavior.destruction_coin_count > 0 && present(behavior.destruction_coin);
            unit_group_kill_coin_rewards += present(behavior.destruction_group_kill_coin);
            unit_destroy_children += behavior.destruction_destroy_children;
            unit_delete_children += behavior.destruction_delete_children;
            unit_create_obstacle += behavior.destruction_create_obstacle;
            unit_draw_to_terrain += behavior.destruction_draw_to_terrain;
            unit_random_bonus += behavior.destruction_release_random_bonus;
            unresolved_active_actions += behavior.unresolved_active_actions;
            unresolved_inert_actions += behavior.unresolved_inert_actions;
            bool unit_has_lock_owner = false;
            bool unit_has_shield_depletion = false;
            bool unit_has_link_owner = false;
            bool unit_has_orbit_owner = false;
            for (std::size_t state_index = 0; state_index < unit->states.size(); ++state_index) {
                const auto& state = unit->states[state_index];
                const auto& compiled_state = behavior.states[state_index];
                const bool expected_collides = state.fields.bool_value("stateCollides_BOOL").value_or(false);
                const bool expected_pass = state.fields.bool_value("passHitsToOwner_BOOL").value_or(false);
                const bool expected_invulnerable = state.fields.bool_value(
                    "stateInvulnerable_ShieldsDoNotDepleteOnCollision_BOOL").value_or(false);
                const bool expected_players = state.fields.bool_value("stateCollidesWithPlayers_BOOL").value_or(false);
                const bool expected_no_glow = state.fields.bool_value("stateDoNotGlowOnCollision_BOOL").value_or(false);
                const auto expected_spawn = state.fields.id_value("collision_Spawn_ID").value_or(deimos::FourCC{});
                const bool expected_shield_depletion = state.fields.bool_value(
                    "stateUseThisStateOnShieldDepletion_BOOL").value_or(false);
                const bool expected_destroy_with_owner = state.fields.bool_value(
                    "canBeDestroyedOnOwnerDestruction_BOOL").value_or(false);
                const bool expected_delete_with_owner = state.fields.bool_value(
                    "canBeDeletedOnOwnerDeletion_BOOL").value_or(false);
                const bool expected_destroy_owner = state.fields.bool_value(
                    "destroyOwnerOnDestruction_BOOL").value_or(false);
                const auto expected_sprite_face = state.fields.id_value("stateSpriteFace_ID").value_or(deimos::FourCC{});
                const int expected_frame_min = state.fields.int_value("stateSpriteFrameMin_INT").value_or(0);
                const int expected_frame_max = state.fields.int_value("stateSpriteFrameMax_INT").value_or(0);
                const bool expected_parent_direction = state.fields.bool_value("stateUseParentDirection_BOOL").value_or(false);
                const int expected_visibility = state.fields.int_value("stateRequiredVisibilityPercent_INT").value_or(0);
                const int expected_visibility_delta = state.fields.int_value("stateVisibilityDeltaPercent_INT").value_or(0);
                const int expected_scale = state.fields.int_value("stateRequiredScalePercent_INT").value_or(0);
                const int expected_scale_delta = state.fields.int_value("stateScaleDeltaPercent_INT").value_or(0);
                const int expected_tint = state.fields.int_value("stateTintPercent_INT").value_or(0);
                const int expected_tint_delta = state.fields.int_value("stateTintDeltaPercent_INT").value_or(0);
                const auto expected_tint_color = state.fields.color_value("stateTintColor_COLOR").value_or(deimos::Rgb24{});
                const bool expected_colorise = state.fields.bool_value("stateDoColorise_BOOL").value_or(false);
                const bool expected_draw_to_terrain = state.fields.bool_value("stateDrawToTerrain_BOOL").value_or(false);
                if (!(compiled_state.sprite_face == expected_sprite_face) ||
                    compiled_state.sprite_frame_min != expected_frame_min ||
                    compiled_state.sprite_frame_max != expected_frame_max ||
                    compiled_state.use_parent_direction != expected_parent_direction ||
                    compiled_state.required_visibility_percent != expected_visibility ||
                    compiled_state.visibility_delta_percent != expected_visibility_delta ||
                    compiled_state.required_scale_percent != expected_scale ||
                    compiled_state.scale_delta_percent != expected_scale_delta ||
                    compiled_state.tint_percent != expected_tint ||
                    compiled_state.tint_delta_percent != expected_tint_delta ||
                    !(compiled_state.tint_color == expected_tint_color) ||
                    compiled_state.do_colorise != expected_colorise ||
                    compiled_state.draw_to_terrain != expected_draw_to_terrain ||
                    compiled_state.collides != expected_collides ||
                    compiled_state.pass_hits_to_owner != expected_pass ||
                    compiled_state.invulnerable_on_collision != expected_invulnerable ||
                    compiled_state.collides_with_players != expected_players ||
                    compiled_state.do_not_glow_on_collision != expected_no_glow ||
                    compiled_state.use_on_shield_depletion != expected_shield_depletion ||
                    !(compiled_state.collision_spawn == expected_spawn) ||
                    compiled_state.can_be_destroyed_on_owner_destruction != expected_destroy_with_owner ||
                    compiled_state.can_be_deleted_on_owner_deletion != expected_delete_with_owner ||
                    compiled_state.destroy_owner_on_destruction != expected_destroy_owner) {
                    std::cerr << entry.path << ": compiled visual/collision/destruction state fields disagree with parsed source\n";
                    return 20;
                }
                state_collides += expected_collides;
                state_pass_hits_owner += expected_pass;
                state_collision_invulnerable += expected_invulnerable;
                state_collides_players += expected_players;
                state_no_collision_glow += expected_no_glow;
                state_collision_spawns += expected_spawn.str() != "none" && !(expected_spawn == deimos::FourCC{});
                state_shield_depletion += expected_shield_depletion;
                unit_has_shield_depletion = unit_has_shield_depletion || expected_shield_depletion;
                state_destroy_with_owner += expected_destroy_with_owner;
                state_delete_with_owner += expected_delete_with_owner;
                state_destroy_owner += expected_destroy_owner;
                state_draw_to_terrain_visual += expected_draw_to_terrain;
                state_do_colorise += expected_colorise;
                state_nondefault_visibility += expected_visibility != 100;
                state_nondefault_scale += expected_scale != 100;
                state_nonzero_tint += expected_tint != 0;

                const bool lock_owner = state.fields.bool_value("stateLockToOwnerLoc_BOOL").value_or(false);
                const bool link_owner = state.fields.bool_value("stateLinkToOwnerLoc_BOOL").value_or(false);
                const bool orbit_owner = state.fields.bool_value("stateOrbitOwner_BOOL").value_or(false);
                state_lock_owner += lock_owner;
                state_link_owner += link_owner;
                state_orbit_owner += orbit_owner;
                state_hunt += state.fields.bool_value("stateHunts_BOOL").value_or(false);
                state_hold += state.fields.bool_value("stateHoldPositionToTarget_BOOL").value_or(false);
                state_cyclic += state.fields.bool_value("stateCyclicMotion_BOOL").value_or(false);
                state_delete_no_player += state.fields.bool_value("stateDeleteOnNoActivePlayers_BOOL").value_or(false);
                state_destruct_no_player += state.fields.bool_value("stateDestructOnNoActivePlayers_BOOL").value_or(false);
                unit_has_lock_owner = unit_has_lock_owner || lock_owner;
                unit_has_link_owner = unit_has_link_owner || link_owner;
                unit_has_orbit_owner = unit_has_orbit_owner || orbit_owner;
                unit_spawn_sets += state.spawn_sets.size();
                unit_rules += state.rules.size();
                for (const auto& spawn : state.spawn_sets) {
                    spawn_repeat += spawn.repeat_spawns;
                    spawn_absolute += spawn.absolute_coordinates;
                    spawn_rotated_offset += spawn.adjust_offset_for_unit_rotation;
                    spawn_offscreen_guard += spawn.dont_spawn_offscreen;
                    spawn_while_fleeing += spawn.spawn_if_fleeing;
                    spawn_set_heading += spawn.set_heading;
                    spawn_pause_rotation += spawn.pause_rotation_while_spawning;
                    spawn_terrain_effects += spawn.terrain_effects_option;
                    if (spawn.rate_max < spawn.rate_min ||
                        spawn.num_in_volley_max < spawn.num_in_volley_min ||
                        spawn.delay_between_entities_max < spawn.delay_between_entities_min) {
                        ++spawn_reversed_ranges;
                    }
                }
            }
            if (behavior.has_shield_depletion_state != unit_has_shield_depletion) {
                std::cerr << entry.path
                          << ": compiled live +0xCD shield-depletion cache disagrees with parsed states\n";
                return 28;
            }
            unit_shield_depletion_state += unit_has_shield_depletion;
            units_with_lock_owner += unit_has_lock_owner;
            units_with_link_owner += unit_has_link_owner;
            units_with_orbit_owner += unit_has_orbit_owner;
            for (const auto& state : behavior.states) {
                for (const auto& rule : state.rules) {
                    if (rule.condition == deimos::UnitRuleConditionKind::unknown) ++unknown_rule_conditions;
                }
            }
        } else if (ext == ".wede") {
            auto weapon = deimos::decode_and_parse_weapon_definition(*bytes, &error);
            if (!weapon) { std::cerr << entry.path << ": " << error << '\n'; return 10; }
            ++weapons;
            weapon_spawns += weapon->spawns.size();
        } else if (ext == ".plde") {
            auto player = deimos::decode_and_parse_player_definition(*bytes, &error);
            if (!player) {
                std::cerr << entry.path << ": " << error << '\n'; return 11;
            }
            const auto runtime = deimos::compile_player_runtime_definition(*player);
            if (runtime.default_shield_percentage != player->fields.float_value("defaultShieldPercentage_INT").value_or(100.0f) ||
                runtime.shield_warning_percentage != player->fields.float_value("shieldWarningPercentage_INT").value_or(15.0f) ||
                runtime.shield_base_hit_percentage != player->fields.float_value("shieldBaseHitPercentage_INT").value_or(15.0f) ||
                runtime.shield_hit_delay_ticks != player->fields.int_value("shieldHitDelay_INT").value_or(1) ||
                runtime.life_max != player->fields.int_value("life_MaxNum_INT").value_or(10) ||
                runtime.life_initial != player->fields.int_value("life_NumInitial_INT").value_or(3) ||
                !(runtime.life_spawn == player->fields.id_value("life_Spawn_ID").value_or(deimos::FourCC{})) ||
                runtime.game_over_time_ticks != player->fields.int_value("gameOverTime_INT").value_or(20) ||
                runtime.dying_time_ticks != player->fields.int_value("dyingTime_INT").value_or(80) ||
                runtime.final_dying_time_ticks != player->fields.int_value("finalDyingTime_INT").value_or(40) ||
                runtime.entry_invulnerability_time_ticks != player->fields.int_value("entry_InvulnerabilityTime_INT").value_or(60) ||
                runtime.entry_solo_start_x != player->fields.int_value("entry_soloStartX_INT").value_or(208) ||
                runtime.entry_solo_start_y != player->fields.int_value("entry_soloStartY_INT").value_or(330) ||
                runtime.entry_multi_start_x != player->fields.int_value("entry_multiStartX_INT").value_or(104) ||
                runtime.entry_multi_start_y != player->fields.int_value("entry_multiStartY_INT").value_or(330) ||
                !(runtime.entry_spawn == player->fields.id_value("entry_Spawn_ID").value_or(deimos::FourCC{})) ||
                runtime.entry_initial_delay_ticks != player->fields.int_value("entry_InitialDelay_INT").value_or(55) ||
                !(runtime.death_spawn == player->fields.id_value("death_Spawn_ID").value_or(deimos::FourCC{})) ||
                !(runtime.active_spawn_on_hit == player->fields.id_value("active_SpawnOnHit_ID").value_or(deimos::FourCC{})) ||
                !(runtime.active_shield_warning_object == player->fields.id_value("active_ShieldWarningObject_ID").value_or(deimos::FourCC{})) ||
                !(runtime.active_defence_bonus_object == player->fields.id_value("active_DefenceBonusObject_ID").value_or(deimos::FourCC{}))) {
                std::cerr << entry.path << ": compiled player-runtime fields disagree with parsed source\n";
                return 25;
            }
            player_runtime_defs.emplace_back(player->name, runtime);
            ++players;
        } else if (ext == ".idli" || ext == ".flli" || ext == ".coli" || ext == ".tefo" ||
                   ext == ".stli" || ext == ".reli") {
            auto doc = deimos::parse_tagged_text(deimos::decode_legacy_text(*bytes), &error);
            if (!doc) { std::cerr << entry.path << ": " << error << '\n'; return 7; }
            bool ok = false;
            if (ext == ".idli") {
                auto table = deimos::parse_id_list(*doc, &error);
                ok = bool(table);
                if (table && entry.path == "idli/Objects[gaob].idli") canonical_game_objects = *table;
                ++id_lists;
            }
            if (ext == ".flli") {
                auto table = deimos::parse_float_list(*doc, &error);
                ok = bool(table);
                if (table && entry.path == "flli/Game[gafl].flli") canonical_game_floats = *table;
                ++float_lists;
            }
            if (ext == ".coli") { ok = bool(deimos::parse_color_list(*doc, &error)); ++color_lists; }
            if (ext == ".tefo") { ok = bool(deimos::parse_text_format(*doc, &error)); ++text_formats; }
            if (ext == ".stli") { ok = bool(deimos::parse_string_list(*doc, &error)); ++string_lists; }
            if (ext == ".reli") { ok = bool(deimos::parse_rect_list(*doc, &error)); ++rect_lists; }
            if (!ok) { std::cerr << entry.path << ": " << error << '\n'; return 8; }
        }
    }


    if (!canonical_game_floats || !canonical_game_objects) {
        std::cerr << "canonical Game[gafl]/Objects[gaob] tables not found\n";
        return 22;
    }
    const auto random_bonus_config = deimos::compile_legacy_random_bonus_config(
        *canonical_game_floats, *canonical_game_objects, &error);
    if (!random_bonus_config) {
        std::cerr << "canonical random-bonus config: " << error << '\n';
        return 23;
    }
    const auto water_impact_config = deimos::compile_legacy_water_impact_config(
        *canonical_game_objects, &error);
    if (!water_impact_config) {
        std::cerr << "canonical water-impact config: " << error << '\n';
        return 24;
    }
    const auto terrain_surface_config = deimos::compile_legacy_terrain_surface_config(
        *canonical_game_floats, &error);
    if (!terrain_surface_config || terrain_surface_config->visible_width != 416 ||
        terrain_surface_config->visible_height != 480 || terrain_surface_config->display_depth != 16) {
        std::cerr << "canonical terrain-surface config: "
                  << (error.empty() ? "unexpected values" : error) << '\n';
        return 31;
    }
    const auto shadow_runtime_config = deimos::compile_legacy_shadow_runtime_config(
        *canonical_game_floats, &error);
    if (!shadow_runtime_config || shadow_runtime_config->air_x_offset != -48.0f ||
        shadow_runtime_config->air_y_offset != 104.0f || shadow_runtime_config->ground_x_offset != -6.0f ||
        shadow_runtime_config->ground_y_offset != 8.0f) {
        std::cerr << "canonical shadow-runtime config: " << (error.empty() ? "unexpected values" : error) << '\n';
        return 25;
    }
    const auto player_runtime_globals = deimos::compile_legacy_player_runtime_globals(
        *canonical_game_floats, &error);
    if (!player_runtime_globals) {
        std::cerr << "canonical player-runtime globals: " << error << '\n';
        return 26;
    }
    const auto player_runtime_resources = deimos::compile_legacy_player_runtime_resources(
        *canonical_game_objects, &error);
    if (!player_runtime_resources) {
        std::cerr << "canonical player-runtime resources: " << error << '\n';
        return 27;
    }

    auto definitions = deimos::GameDefinitions::load_from_game_pak(*pak, &error);
    if (!definitions) {
        std::cerr << "definition database: " << error << '\n';
        return 12;
    }
    const auto reference_issues = definitions->validate_unit_references();

    // Recovered PPC player subsystem: two slots, status==4 active. Use one
    // deterministic world for constructor/tick corpus validation.
    deimos::PlayerWorld simulation_players;
    simulation_players.slots()[0] = {4, 300.0f, 250.0f, 0};
    simulation_players.slots()[1] = {4, -120.0f, 420.0f, 1};

    // Validate the PPC 0x37930 / 0x37B50 initial member math against every
    // canonical Unit Definition independently of group appearance rolls.
    std::size_t constructor_math_units = 0;
    std::size_t constructor_hunt_units = 0;
    std::size_t constructor_random_location_units = 0;
    std::size_t constructor_variable_speed_units = 0;
    std::size_t constructor_reversed_axis_ranges = 0;
    const deimos::LegacyTrigTables constructor_trig;

    for (const auto& tagged : definitions->units()) {
        deimos::SpawnRequestSeed request;
        request.unit_id = tagged.id;
        request.x = 100.0f;
        request.y = 200.0f;
        request.editor_heading_degrees = 180;
        const auto group = deimos::build_entity_group_runtime(request, 1, 0, 0);

        const bool heading_mode = tagged.definition.core_fields
            .bool_value("initialHeadingSetInEditor_BOOL").value_or(false);
        const int supplied_heading = request.editor_heading_degrees;
        deimos::LegacyRandom member_rng(1);
        const int pre_heading = deimos::choose_initial_member_heading(
            tagged.definition, heading_mode, supplied_heading, member_rng);
        const auto position = deimos::choose_initial_member_position(
            tagged.definition, group, member_rng, constructor_trig);

        deimos::EntityInitialMotionFacts motion_facts;
        if (const auto target = simulation_players.closest_active_player(
                position.position.x, position.position.y)) {
            motion_facts.hunt_target_position = target->position;
        }
        const auto motion = deimos::choose_initial_member_motion(
            tagged.definition, group, position.position, false,
            heading_mode, pre_heading, 1.0f,
            motion_facts, member_rng, constructor_trig);
        if (motion.status != deimos::EntityInitialMotionStatus::complete) {
            std::cerr << tagged.path << ": canonical initial-motion path is not reconstructed\n";
            return 14;
        }
        ++constructor_math_units;

        const auto& fields = tagged.definition.core_fields;
        constructor_hunt_units += fields.bool_value("initiallyHuntsClosestPlayer_BOOL").value_or(false);
        constructor_random_location_units += fields.bool_value("randomiseInitialLoc_BOOL").value_or(false);
        const float speed_min = fields.float_value("initialSpeedMin_FLOAT").value_or(0.0f);
        const float speed_max = fields.float_value("initialSpeedMax_FLOAT").value_or(speed_min);
        constructor_variable_speed_units += speed_min != speed_max;
        const float x_min = fields.float_value("xOffsetMin_FLOAT").value_or(0.0f);
        const float x_max = fields.float_value("xOffsetMax_FLOAT").value_or(x_min);
        const float y_min = fields.float_value("yOffsetMin_FLOAT").value_or(0.0f);
        const float y_max = fields.float_value("yOffsetMax_FLOAT").value_or(y_min);
        constructor_reversed_axis_ranges += x_max < x_min;
        constructor_reversed_axis_ranges += y_max < y_min;
    }

    // Exercise the complete currently-recovered normal group/member bridge
    // with one shared RNG/identity stream, as the real game does. Appearance
    // may legitimately reduce a request to zero members; that is not failure.
    std::size_t group_requests = 0;
    std::size_t group_constructed = 0;
    std::size_t group_rejected_by_appearance = 0;
    std::size_t live_members_constructed = 0;
    std::size_t member_spawn_runtime_records = 0;
    std::size_t delete_existing_owner_intents = 0;
    deimos::LegacyRandom construction_rng(1);
    deimos::EntityIdentityCounters identities;
    identities.next_member_handle = 1;
    deimos::EntityWorld constructed_world;

    for (const auto& tagged : definitions->units()) {
        ++group_requests;
        deimos::SpawnRequestSeed request;
        request.unit_id = tagged.id;
        request.x = 100.0f;
        request.y = 200.0f;
        request.editor_heading_degrees = 180;
        request.player_owner_index = 0;

        deimos::EntityHeadlessConstructionContext context;
        context.preflight.current_tick = 0;
        context.preflight.player_gate.global_gate_enabled = true;
        context.preflight.player_gate.qualifying_player_present = simulation_players.any_active_player();
        context.preflight.player_gate.suppression_active = false;
        context.hunt_target_provider = [&](deimos::EntityPoint position) -> std::optional<deimos::EntityPoint> {
            const auto target = simulation_players.closest_active_player(position.x, position.y);
            return target ? std::optional<deimos::EntityPoint>{target->position} : std::nullopt;
        };
        context.motion_facts.parent_heading_degrees = 180;

        auto built = deimos::construct_entity_group_headless(
            tagged.definition, request, context, identities,
            construction_rng, constructor_trig);
        if (built.status == deimos::EntityGroupBuildStatus::rejected) {
            if (built.plan.rejection == deimos::EntityConstructionRejection::no_group_members) {
                ++group_rejected_by_appearance;
                continue;
            }
            std::cerr << tagged.path << ": unexpected constructor rejection\n";
            return 15;
        }
        if (!built.constructed()) {
            std::cerr << tagged.path << ": incomplete canonical constructor branch\n";
            return 16;
        }
        ++group_constructed;
        delete_existing_owner_intents += built.plan.delete_existing_owned_type;
        live_members_constructed += built.members.size();
        for (const auto& member : built.members) {
            if (member.shields != member.behavior.shields_base) {
                std::cerr << tagged.path << ": headless constructor did not initialize base shields\n";
                return 21;
            }
            if (member.state.current_state < member.spawn_runtime_by_state.size()) {
                member_spawn_runtime_records += member.spawn_runtime_by_state[
                    member.state.current_state].spawn_sets.size();
            }
        }
        constructed_world.register_group(std::move(built));
    }

    if (constructed_world.active_member_count() != live_members_constructed ||
        constructed_world.groups().size() != group_constructed) {
        std::cerr << "clean world registry count does not match constructed corpus\n";
        return 17;
    }


    const auto active_members_before_first_tick = constructed_world.active_member_count();

    // Exercise one reconstructed player-aware tick at the same initial tick.
    // This intentionally triggers zero-delay timer actions exactly as the PPC
    // equality check does and classifies every lifecycle change by phase.
    std::size_t player_aware_ticks = 0;
    std::size_t removed_on_first_tick = 0;
    std::size_t first_tick_deleted = 0;
    std::size_t first_tick_destroyed = 0;
    std::size_t removed_by_timer = 0;
    std::size_t removed_by_rule = 0;
    std::size_t removed_by_range = 0;
    std::size_t removed_by_player_motion = 0;
    deimos::LegacyRandom motion_rng(1);

    for (auto& member : constructed_world.members()) {
        if (member.lifecycle != deimos::EntityLifecycle::active) continue;
        const auto* definition = definitions->find_unit(member.unit_id);
        if (!definition) {
            std::cerr << "constructed world member has unknown unit ID\n";
            return 18;
        }
        const auto before = member.lifecycle;
        deimos::EntityTickContext tick_context;
        tick_context.current_tick = 0;
        const auto tick_result = deimos::advance_entity_runtime_with_players(
            constructed_world, member, *definition, tick_context,
            simulation_players, motion_rng, constructor_trig);
        ++player_aware_ticks;
        if (before == deimos::EntityLifecycle::active &&
            member.lifecycle != deimos::EntityLifecycle::active) {
            ++removed_on_first_tick;
            first_tick_deleted += member.lifecycle == deimos::EntityLifecycle::deleted;
            first_tick_destroyed += member.lifecycle == deimos::EntityLifecycle::destroyed;
            if (tick_result.timer_action_processed) ++removed_by_timer;
            else if (tick_result.rule_matched) ++removed_by_rule;
            else if (tick_result.range_action_processed) ++removed_by_range;
            else ++removed_by_player_motion;
        }
    }

    std::size_t sprite_surface_frames = 0;
    std::size_t sprite_pair_casefold_matches = 0;
    std::size_t sprite_transparency_plane_frames = 0;
    std::size_t sprite_color_words = 0;
    std::size_t sprite_transparency_words = 0;
    std::size_t sprite_transparent_row_sentinels = 0;
    std::uint64_t sprite_surface_fnv64 = 1469598103934665603ull;
    std::uint64_t sprite_render_fnv64 = 1469598103934665603ull;
    std::size_t sprite_render_passes = 0;
    for (const auto& [name, alpha_dimensions] : sprite_alpha_dimensions) {
        const auto color = sprite_color_dimensions.find(name);
        if (color == sprite_color_dimensions.end()) continue; // PDLI is alpha-only in stock Game.pak.
        ++sprite_plate_pairs;
        if (color->second != alpha_dimensions) {
            std::cerr << "sprite alpha/color plate dimensions disagree for " << name << '\n';
            return 22;
        }
        const auto alpha_image = sprite_alpha_images.find(name);
        const auto color_image = sprite_color_images.find(name);
        if (alpha_image == sprite_alpha_images.end() || color_image == sprite_color_images.end()) {
            std::cerr << "sprite alpha/color plate image missing for " << name << '\n';
            return 28;
        }
        auto alpha_tag = alpha_image->second.first.str();
        auto color_tag = color_image->second.first.str();
        std::transform(alpha_tag.begin(), alpha_tag.end(), alpha_tag.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        std::transform(color_tag.begin(), color_tag.end(), color_tag.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        if (alpha_tag == color_tag) ++sprite_pair_casefold_matches;
        auto group = deimos::build_legacy_sprite_group(
            alpha_image->second.first, alpha_image->second.second, color_image->second.second, &error);
        if (!group) {
            std::cerr << name << ": legacy sprite-surface construction failed: " << error << '\n';
            return 29;
        }
        const auto tag = group->id.str();
        sprite_surface_fnv64 = fnv1a64_bytes(
            sprite_surface_fnv64,
            std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(tag.data()), tag.size()));
        sprite_surface_fnv64 = fnv1a64_u32(sprite_surface_fnv64, static_cast<std::uint32_t>(group->frames.size()));
        sprite_surface_frames += group->frames.size();
        for (const auto& frame : group->frames) {
            if (!frame.has_surface()) {
                std::cerr << name << ": reconstructed frame surface is incomplete\n";
                return 30;
            }
            sprite_surface_fnv64 = fnv1a64_u32(sprite_surface_fnv64, static_cast<std::uint32_t>(frame.width));
            sprite_surface_fnv64 = fnv1a64_u32(sprite_surface_fnv64, static_cast<std::uint32_t>(frame.height));
            sprite_surface_fnv64 = fnv1a64_u16(sprite_surface_fnv64, frame.transparent_key);
            sprite_color_words += frame.color_pixels.size();
            for (const auto pixel : frame.color_pixels) {
                sprite_surface_fnv64 = fnv1a64_u16(sprite_surface_fnv64, pixel);
            }
            sprite_surface_fnv64 = fnv1a64_u32(
                sprite_surface_fnv64, static_cast<std::uint32_t>(frame.transparency.size()));
            if (frame.has_transparency_plane()) ++sprite_transparency_plane_frames;
            sprite_transparency_words += frame.transparency.size();
            for (const auto value : frame.transparency) {
                sprite_surface_fnv64 = fnv1a64_u16(sprite_surface_fnv64, value);
                sprite_transparent_row_sentinels += value == 1000u;
            }

            // Stress the recovered software compositor against every canonical
            // frame surface. This is a clean-runtime regression oracle over
            // original source pixels/masks, not a claim that the hash itself
            // existed in the Mac executable.
            const auto hash_render = [&](std::uint32_t mode_tag, std::uint32_t flags,
                                         int amount, float scale, std::uint16_t effect_color,
                                         bool alpha_drawing) {
                const int scaled_w = std::max(1, static_cast<int>(std::ceil(static_cast<float>(frame.width) * scale)));
                const int scaled_h = std::max(1, static_cast<int>(std::ceil(static_cast<float>(frame.height) * scale)));
                deimos::LegacyRasterSurface surface(scaled_w + 8, scaled_h + 8);
                for (int y = 0; y < surface.height; ++y) {
                    for (int x = 0; x < surface.width; ++x) {
                        const int r = (x * 3 + y * 5 + static_cast<int>(mode_tag)) & 31;
                        const int g = (x * 7 + y * 2 + static_cast<int>(mode_tag * 3u)) & 31;
                        const int b = (x + y * 11 + static_cast<int>(mode_tag * 5u)) & 31;
                        surface.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(surface.width) +
                                       static_cast<std::size_t>(x)] =
                            static_cast<std::uint16_t>((r << 10) | (g << 5) | b);
                    }
                }
                deimos::LegacyRasterRequest request;
                request.frame = &frame;
                request.center_x = surface.width / 2;
                request.center_y = surface.height / 2;
                request.sprite_face = group->id;
                request.sprite_frame = 0;
                request.flags = flags;
                request.scale = scale;
                request.effect_amount_0_to_32 = amount;
                request.clip = surface.bounds();
                request.immediate = true;
                request.effect_color = effect_color;
                const auto result = deimos::rasterize_legacy_request(
                    request, surface, deimos::LegacyRasterConfig{alpha_drawing, true});
                sprite_render_fnv64 = fnv1a64_u32(sprite_render_fnv64, mode_tag);
                sprite_render_fnv64 = fnv1a64_u32(sprite_render_fnv64, static_cast<std::uint32_t>(result));
                sprite_render_fnv64 = fnv1a64_u32(sprite_render_fnv64, static_cast<std::uint32_t>(surface.width));
                sprite_render_fnv64 = fnv1a64_u32(sprite_render_fnv64, static_cast<std::uint32_t>(surface.height));
                for (const auto pixel : surface.pixels) {
                    sprite_render_fnv64 = fnv1a64_u16(sprite_render_fnv64, pixel);
                }
                ++sprite_render_passes;
            };
            hash_render(0, 0, 0, 1.0f, 0, true);
            hash_render(1, deimos::kLegacyRenderOverallTransparency, 7, 1.0f, 0, true);
            hash_render(2, deimos::kLegacyRenderShadow, 20, 1.0f, 0, true);
            hash_render(3, deimos::kLegacyRenderSolidColor, 9, 1.0f, 0x5294u, true);
            hash_render(4, 0, 0, 1.5f, 0, true);
            hash_render(5, 0, 0, 1.0f, 0, false);
        }
    }
    constexpr std::uint64_t expected_sprite_surface_fnv64 = 0x9f9dcfba05b5089cull;
    constexpr std::size_t expected_sprite_render_passes = 14760;
    constexpr std::uint64_t expected_sprite_render_fnv64 = 0x32290b39b091e970ull;
    if (sprite_alpha_plates != 124 || sprite_color_plates != 124 || sprite_plate_pairs != 123 ||
        sprite_pair_casefold_matches != 123 || sprite_frames != 2463 || sprite_surface_frames != 2460 ||
        sprite_transparency_plane_frames != 2460 || sprite_color_words != 3115564 ||
        sprite_transparency_words != 3115564 || sprite_transparent_row_sentinels != 6341 ||
        sprite_surface_fnv64 != expected_sprite_surface_fnv64 ||
        sprite_render_passes != expected_sprite_render_passes ||
        sprite_render_fnv64 != expected_sprite_render_fnv64 ||
        pl1b_frames != 7 || pl1b_frame0 != std::pair<int,int>{53,43} ||
        exlg_frames != 12 || bocr_frames != 3 || glow_frames != 12) {
        std::cerr << "canonical sprite plate/frame/surface/software-render contract changed unexpectedly\n";
        return 23;
    }

    if (!reference_issues.empty()) {
        for (const auto& issue : reference_issues) {
            std::cerr << issue.source_path << ": unresolved Unit Definition reference "
                      << issue.field << " -> [" << issue.target.str() << "]\n";
        }
        return 13;
    }

    std::cout << "Game.pak clean-core validation PASS\n"
              << "  actual files: " << files << '\n'
              << "  levels: " << levels << '\n'
              << "  placed objects: " << objects << '\n'
              << "  films: " << films << '\n'
              << "  ID lists: " << id_lists << '\n'
              << "  float lists: " << float_lists << '\n'
              << "  color lists: " << color_lists << '\n'
              << "  text formats: " << text_formats << '\n'
              << "  string lists: " << string_lists << '\n'
              << "  rect lists: " << rect_lists << '\n'
              << "  units: " << units << '\n'
              << "  unit states: " << unit_states << '\n'
              << "    Lock-to-owner states: " << state_lock_owner
              << " across " << units_with_lock_owner << " units\n"
              << "    Link-to-owner states: " << state_link_owner
              << " across " << units_with_link_owner << " units\n"
              << "    Orbit-owner states: " << state_orbit_owner
              << " across " << units_with_orbit_owner << " units\n"
              << "    Hunt states: " << state_hunt << '\n'
              << "    Hold-to-target states: " << state_hold << '\n'
              << "    Cyclic-motion states: " << state_cyclic << '\n'
              << "    Delete-on-no-player states: " << state_delete_no_player << '\n'
              << "    Destruct-on-no-player states: " << state_destruct_no_player << '\n'
              << "    Collision-enabled states: " << state_collides << '\n'
              << "    Pass-hits-to-owner states: " << state_pass_hits_owner << '\n'
              << "    Collision-invulnerable states: " << state_collision_invulnerable << '\n'
              << "    Player-collision states: " << state_collides_players << '\n'
              << "    No-glow-on-collision states: " << state_no_collision_glow << '\n'
              << "    Collision-spawn states: " << state_collision_spawns << '\n'
              << "    Shield-depletion states: " << state_shield_depletion << '\n'
              << "    Units with shield-depletion state: " << unit_shield_depletion_state << '\n'
              << "  collision domains: air=" << unit_air_collision_domain
              << " ground=" << unit_ground_collision_domain << '\n'
              << "    harmless units: " << unit_harmless << '\n'
              << "    player-projectile units: " << unit_player_projectile << '\n'
              << "    projectile-hittable units: " << unit_player_projectile_hittable << '\n'
              << "    nonzero collision damage: " << unit_nonzero_collision_damage << '\n'
              << "    nonzero base shields: " << unit_nonzero_shields << '\n'
              << "    pickup units: " << unit_pickups << '\n'
              << "      coin=" << pickup_coin
              << " mult=" << pickup_multiplier
              << " exli=" << pickup_extra_life
              << " shie=" << pickup_shield
              << " air=" << pickup_air
              << " grnd=" << pickup_ground
              << " spec=" << pickup_special
              << " other=" << pickup_other << '\n'
              << "  player lifecycle definitions:\n";
    for (const auto& [name, runtime] : player_runtime_defs) {
        std::cout << "    " << name
                  << ": timers="
                  << runtime.game_over_time_ticks << '/'
                  << runtime.dying_time_ticks << '/'
                  << runtime.final_dying_time_ticks << '/'
                  << runtime.entry_initial_delay_ticks << '/'
                  << runtime.entry_invulnerability_time_ticks
                  << " solo=" << runtime.entry_solo_start_x << ',' << runtime.entry_solo_start_y
                  << " multi=" << runtime.entry_multi_start_x << ',' << runtime.entry_multi_start_y
                  << " entry=" << runtime.entry_spawn.str() << '\n';
    }
    std::cout << "  player runtime globals:\n"
              << "    Player_ImpactDamageToEntities: " << player_runtime_globals->impact_damage_to_entities << '\n'
              << "    Player_DelayBetweenHitSpawns: " << player_runtime_globals->delay_between_hit_spawns << '\n'
              << "    Entity_HitDelay: " << player_runtime_globals->entity_hit_delay_ticks << '\n'
              << "    death-money IDs: "
              << player_runtime_resources->money_50.str() << ','
              << player_runtime_resources->money_10.str() << ','
              << player_runtime_resources->money_5.str() << ','
              << player_runtime_resources->money_1.str() << '\n'
              << "  audio/music resources:\n"
              << "    canonical SFX AIFC/ima4 resources validated: " << audio_resources_validated << '\n'
              << "    canonical SFX decoded frames: " << audio_frames_validated << '\n'
              << "    canonical music AIFC/ima4 resources validated: " << music_resources_validated << '\n'
              << "  sprite resource cache/plates:\n"
              << "    alpha plates: " << sprite_alpha_plates << '\n'
              << "    color plates: " << sprite_color_plates << '\n'
              << "    matched alpha/color pairs: " << sprite_plate_pairs << '\n'
              << "    extracted alpha frames: " << sprite_frames << '\n'
              << "    reconstructed 16-bit frame surfaces: " << sprite_surface_frames << '\n'
              << "    alpha/color case-fold tag matches: " << sprite_pair_casefold_matches << '\n'
              << "    frames with transparency plane: " << sprite_transparency_plane_frames << '\n'
              << "    frame color/transparency words: " << sprite_color_words << '/' << sprite_transparency_words << '\n'
              << "    transparent-row sentinels: " << sprite_transparent_row_sentinels << '\n'
              << "    sprite-surface FNV64: 0x" << std::hex << sprite_surface_fnv64 << std::dec << '\n'
              << "    canonical software-render passes: " << sprite_render_passes << '\n'
              << "    canonical software-render FNV64: 0x" << std::hex << sprite_render_fnv64 << std::dec << '\n'
              << "    PL1B frames/frame0: " << pl1b_frames << '/' << pl1b_frame0.first << 'x' << pl1b_frame0.second << '\n'
              << "    EXLG/BOCR/GLOW frames: " << exlg_frames << '/' << bocr_frames << '/' << glow_frames << '\n'
              << "  visual/render fields:\n"
              << "    shadow offsets air/ground: "
              << shadow_runtime_config->air_x_offset << ',' << shadow_runtime_config->air_y_offset << " / "
              << shadow_runtime_config->ground_x_offset << ',' << shadow_runtime_config->ground_y_offset << '\n'
              << "    scale-tolerance units: " << unit_scale_tolerance << '\n'
              << "    adjust-shadow-for-scaling units: " << unit_adjust_shadow_scaling << '\n'
              << "    draw-to-terrain states: " << state_draw_to_terrain_visual << '\n'
              << "    colorise states: " << state_do_colorise << '\n'
              << "    non-100 visibility states: " << state_nondefault_visibility << '\n'
              << "    non-100 scale states: " << state_nondefault_scale << '\n'
              << "    nonzero tint states: " << state_nonzero_tint << '\n'
              << "    draw layers: defa=" << layer_defa
              << " grou=" << layer_grou
              << " grhi=" << layer_grhi
              << " ailo=" << layer_ailo
              << " aihi=" << layer_aihi
              << " plwe=" << layer_plwe
              << " play=" << layer_play
              << " plsh=" << layer_plsh
              << " plef=" << layer_plef
              << " plui=" << layer_plui
              << " atmo=" << layer_atmo
              << " hud=" << layer_hud
              << " none=" << layer_none
              << " other=" << layer_other << '\n'
              << "  terrain/media fields:\n"
              << "    casts-shadows units: " << unit_casts_shadows << '\n'
              << "    ground-obstacle-collision units: " << unit_ground_obstacle_collision << '\n'
              << "    death-spawn-any-media units: " << unit_death_spawn_any_media << '\n'
              << "    non-none media-impact-size units: " << unit_media_impact_size << '\n'
              << "    water impact IDs: "
              << water_impact_config->tiny.str() << ','
              << water_impact_config->small.str() << ','
              << water_impact_config->medium.str() << ','
              << water_impact_config->large.str() << '\n'
              << "    terrain viewport/depth: "
              << terrain_surface_config->visible_width << 'x'
              << terrain_surface_config->visible_height << 'x'
              << terrain_surface_config->display_depth << '\n'
              << "  destruction/removal fields:\n"
              << "    destruction spawns: " << unit_destruction_spawns << '\n'
              << "    deletion spawns: " << unit_deletion_spawns << '\n'
              << "    destruction particle effects: " << unit_destruction_particles << '\n'
              << "    destruction notices: " << unit_destruction_notices << '\n'
              << "    destruction sounds: " << unit_destruction_sounds << '\n'
              << "    ordinary coin reward units: " << unit_destruction_coin_rewards << '\n'
              << "    group-kill coin reward units: " << unit_group_kill_coin_rewards << '\n'
              << "    destroy-children units: " << unit_destroy_children << '\n'
              << "    delete-children units: " << unit_delete_children << '\n'
              << "    create-obstacle units: " << unit_create_obstacle << '\n'
              << "    draw-to-terrain units: " << unit_draw_to_terrain << '\n'
              << "    random-bonus units: " << unit_random_bonus << '\n'
              << "    states destroyed with owner: " << state_destroy_with_owner << '\n'
              << "    states deleted with owner: " << state_delete_with_owner << '\n'
              << "    states that destroy owner: " << state_destroy_owner << '\n'
              << "    random bonus thresholds: "
              << random_bonus_config->percent_thresholds[0] << ','
              << random_bonus_config->percent_thresholds[1] << ','
              << random_bonus_config->percent_thresholds[2] << ','
              << random_bonus_config->percent_thresholds[3] << ','
              << random_bonus_config->percent_thresholds[4] << ','
              << random_bonus_config->percent_thresholds[5] << ','
              << random_bonus_config->percent_thresholds[6] << ','
              << random_bonus_config->percent_thresholds[7] << ','
              << random_bonus_config->percent_thresholds[8] << '\n'
              << "    ground-accuracy random-bonus threshold: "
              << random_bonus_config->ground_accuracy_reward_percent << '\n'
              << "    highest random-bonus minimum progression: "
              << random_bonus_config->minimum_progression_for_highest_bonus << '\n'
              << "  unit terrain effects: " << unit_terrain_effects << '\n'
              << "  unit owner-scale spawn-offset flag: " << unit_adjust_owner_scale << '\n'
              << "  unit player-active-only spawn flag: " << unit_player_active_only << '\n'
              << "  unit spawn sets: " << unit_spawn_sets << '\n'
              << "    repeating: " << spawn_repeat << '\n'
              << "    absolute coordinates: " << spawn_absolute << '\n'
              << "    rotation-adjusted offsets: " << spawn_rotated_offset << '\n'
              << "    offscreen guard: " << spawn_offscreen_guard << '\n'
              << "    spawn while fleeing: " << spawn_while_fleeing << '\n'
              << "    set heading: " << spawn_set_heading << '\n'
              << "    pause rotation while spawning: " << spawn_pause_rotation << '\n'
              << "    terrain-effects option: " << spawn_terrain_effects << '\n'
              << "    reversed numeric ranges: " << spawn_reversed_ranges << '\n'
              << "  unit rules: " << unit_rules << '\n'
              << "  weapons: " << weapons << '\n'
              << "  weapon spawns: " << weapon_spawns << '\n'
              << "  players: " << players << '\n'
              << "  active unresolved/no-op state actions: " << unresolved_active_actions << '\n'
              << "  inert unresolved state actions: " << unresolved_inert_actions << '\n'
              << "  unknown rule conditions: " << unknown_rule_conditions << '\n'
              << "  unit-reference issues: " << reference_issues.size() << '\n'
              << "  initial-member math validated: " << constructor_math_units << '\n'
              << "    initially-hunting units: " << constructor_hunt_units << '\n'
              << "    randomized-location units: " << constructor_random_location_units << '\n'
              << "    variable-speed units: " << constructor_variable_speed_units << '\n'
              << "    reversed X/Y offset ranges: " << constructor_reversed_axis_ranges << '\n'
              << "  headless normal-path group requests: " << group_requests << '\n'
              << "    groups constructed: " << group_constructed << '\n'
              << "    groups eliminated by appearance rolls: " << group_rejected_by_appearance << '\n'
              << "    live members constructed: " << live_members_constructed << '\n'
              << "    world-registered active members: " << active_members_before_first_tick << '\n'
              << "    spawn records in resulting current states: " << member_spawn_runtime_records << '\n'
              << "    delete-existing-owner intents: " << delete_existing_owner_intents << '\n'
              << "    next group serial: " << identities.next_group_serial << '\n'
              << "    next member serial: " << identities.next_member_serial << '\n'
              << "    final construction RNG seed: " << construction_rng.seed() << '\n'
              << "  player-aware first ticks: " << player_aware_ticks << '\n'
              << "    active after first tick: " << constructed_world.active_member_count() << '\n'
              << "    removed on first tick: " << removed_on_first_tick << '\n'
              << "      deleted: " << first_tick_deleted << '\n'
              << "      destroyed: " << first_tick_destroyed << '\n'
              << "      timer phase: " << removed_by_timer << '\n'
              << "      rule phase: " << removed_by_rule << '\n'
              << "      range phase: " << removed_by_range << '\n'
              << "      player/motion phase: " << removed_by_player_motion << '\n'
              << "    final motion RNG seed: " << motion_rng.seed() << '\n';
    return 0;
}
