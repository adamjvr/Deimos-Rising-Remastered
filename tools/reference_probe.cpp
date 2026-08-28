#include "deimos/data_tables.hpp"
#include "deimos/entity_runtime.hpp"
#include "deimos/entity_world.hpp"
#include "deimos/film.hpp"
#include "deimos/game_definitions.hpp"
#include "deimos/legacy_text.hpp"
#include "deimos/level.hpp"
#include "deimos/pak_archive.hpp"
#include "deimos/player_definition.hpp"
#include "deimos/unit_definition.hpp"
#include "deimos/unit_behavior.hpp"
#include "deimos/weapon_definition.hpp"

#include <filesystem>
#include <iostream>
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
    std::size_t units_with_lock_owner = 0, units_with_link_owner = 0, units_with_orbit_owner = 0;
    std::size_t weapons = 0, weapon_spawns = 0, players = 0;
    std::size_t unresolved_active_actions = 0, unresolved_inert_actions = 0, unknown_rule_conditions = 0;

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
            unresolved_active_actions += behavior.unresolved_active_actions;
            unresolved_inert_actions += behavior.unresolved_inert_actions;
            bool unit_has_lock_owner = false;
            bool unit_has_link_owner = false;
            bool unit_has_orbit_owner = false;
            for (const auto& state : unit->states) {
                const bool lock_owner = state.fields.bool_value("stateLockToOwnerLoc_BOOL").value_or(false);
                const bool link_owner = state.fields.bool_value("stateLinkToOwnerLoc_BOOL").value_or(false);
                const bool orbit_owner = state.fields.bool_value("stateOrbitOwner_BOOL").value_or(false);
                state_lock_owner += lock_owner;
                state_link_owner += link_owner;
                state_orbit_owner += orbit_owner;
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
            if (ext == ".idli") { ok = bool(deimos::parse_id_list(*doc, &error)); ++id_lists; }
            if (ext == ".flli") { ok = bool(deimos::parse_float_list(*doc, &error)); ++float_lists; }
            if (ext == ".coli") { ok = bool(deimos::parse_color_list(*doc, &error)); ++color_lists; }
            if (ext == ".tefo") { ok = bool(deimos::parse_text_format(*doc, &error)); ++text_formats; }
            if (ext == ".stli") { ok = bool(deimos::parse_string_list(*doc, &error)); ++string_lists; }
            if (ext == ".reli") { ok = bool(deimos::parse_rect_list(*doc, &error)); ++rect_lists; }
            if (!ok) { std::cerr << entry.path << ": " << error << '\n'; return 8; }
        }
    }


    auto definitions = deimos::GameDefinitions::load_from_game_pak(*pak, &error);
    if (!definitions) {
        std::cerr << "definition database: " << error << '\n';
        return 12;
    }
    const auto reference_issues = definitions->validate_unit_references();

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
        // Only one canonical definition (Mine[mine]) uses the hunt branch.
        // Supplying a deterministic target validates the recovered vector math
        // without pretending that target selection itself is reconstructed yet.
        motion_facts.hunt_target_position = deimos::EntityPoint{
            position.position.x + 100.0f,
            position.position.y + 50.0f};
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
        context.preflight.player_gate.qualifying_player_present = true;
        context.preflight.player_gate.suppression_active = false;
        context.motion_facts.hunt_target_position = deimos::EntityPoint{300.0f, 250.0f};
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
              << "    world-registered active members: " << constructed_world.active_member_count() << '\n'
              << "    spawn records in resulting current states: " << member_spawn_runtime_records << '\n'
              << "    delete-existing-owner intents: " << delete_existing_owner_intents << '\n'
              << "    next group serial: " << identities.next_group_serial << '\n'
              << "    next member serial: " << identities.next_member_serial << '\n'
              << "    final construction RNG seed: " << construction_rng.seed() << '\n';
    return 0;
}
