#include "deimos/data_tables.hpp"
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
            for (const auto& state : unit->states) {
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
              << "  unit-reference issues: " << reference_issues.size() << '\n';
    return 0;
}
