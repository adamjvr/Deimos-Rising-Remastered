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
#include "deimos/terrain_runtime.hpp"
#include "deimos/unit_definition.hpp"
#include "deimos/unit_behavior.hpp"
#include "deimos/weapon_definition.hpp"

#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <utility>

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
    std::size_t unit_ground_collision_domain = 0, unit_air_collision_domain = 0;
    std::size_t unit_harmless = 0, unit_player_projectile = 0, unit_player_projectile_hittable = 0;
    std::size_t unit_nonzero_collision_damage = 0, unit_nonzero_shields = 0;
    std::size_t unit_pickups = 0;
    std::size_t unit_casts_shadows = 0, unit_ground_obstacle_collision = 0;
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
    std::size_t weapons = 0, weapon_spawns = 0, players = 0;
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
            if (!(behavior.collision_domain == expected_domain) ||
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
                std::cerr << entry.path << ": compiled collision/destruction UnitDef fields disagree with parsed source\n";
                return 19;
            }
            unit_ground_collision_domain += ground;
            unit_air_collision_domain += !ground;
            unit_harmless += behavior.harmless_to_players;
            unit_player_projectile += behavior.player_projectile;
            unit_player_projectile_hittable += behavior.can_be_hit_by_player_projectile;
            unit_nonzero_collision_damage += behavior.collision_damage != 0.0f;
            unit_nonzero_shields += behavior.shields_base != 0.0f;
            unit_pickups += behavior.pickup_type.str() != "none" && !(behavior.pickup_type == deimos::FourCC{});
            const auto present = [](deimos::FourCC value) {
                return !(value == deimos::FourCC{}) && value.str() != "none" && value.str() != "NULL";
            };
            unit_casts_shadows += behavior.casts_shadows;
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
                const bool expected_destroy_with_owner = state.fields.bool_value(
                    "canBeDestroyedOnOwnerDestruction_BOOL").value_or(false);
                const bool expected_delete_with_owner = state.fields.bool_value(
                    "canBeDeletedOnOwnerDeletion_BOOL").value_or(false);
                const bool expected_destroy_owner = state.fields.bool_value(
                    "destroyOwnerOnDestruction_BOOL").value_or(false);
                if (compiled_state.collides != expected_collides ||
                    compiled_state.pass_hits_to_owner != expected_pass ||
                    compiled_state.invulnerable_on_collision != expected_invulnerable ||
                    compiled_state.collides_with_players != expected_players ||
                    compiled_state.do_not_glow_on_collision != expected_no_glow ||
                    !(compiled_state.collision_spawn == expected_spawn) ||
                    compiled_state.can_be_destroyed_on_owner_destruction != expected_destroy_with_owner ||
                    compiled_state.can_be_deleted_on_owner_deletion != expected_delete_with_owner ||
                    compiled_state.destroy_owner_on_destruction != expected_destroy_owner) {
                    std::cerr << entry.path << ": compiled collision/destruction state fields disagree with parsed source\n";
                    return 20;
                }
                state_collides += expected_collides;
                state_pass_hits_owner += expected_pass;
                state_collision_invulnerable += expected_invulnerable;
                state_collides_players += expected_players;
                state_no_collision_glow += expected_no_glow;
                state_collision_spawns += expected_spawn.str() != "none" && !(expected_spawn == deimos::FourCC{});
                state_destroy_with_owner += expected_destroy_with_owner;
                state_delete_with_owner += expected_delete_with_owner;
                state_destroy_owner += expected_destroy_owner;

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
            if (!deimos::decode_and_parse_player_definition(*bytes, &error)) {
                std::cerr << entry.path << ": " << error << '\n'; return 11;
            }
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
              << "  collision domains: air=" << unit_air_collision_domain
              << " ground=" << unit_ground_collision_domain << '\n'
              << "    harmless units: " << unit_harmless << '\n'
              << "    player-projectile units: " << unit_player_projectile << '\n'
              << "    projectile-hittable units: " << unit_player_projectile_hittable << '\n'
              << "    nonzero collision damage: " << unit_nonzero_collision_damage << '\n'
              << "    nonzero base shields: " << unit_nonzero_shields << '\n'
              << "    pickup units: " << unit_pickups << '\n'
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
