#pragma once

#include "deimos/definition_fields.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace deimos {

struct UnitSpawnSet {
    std::string name;
    FourCC spawn_id{};
    int x_offset = 0;
    int y_offset = 0;
    bool adjust_offset_for_unit_rotation = false;
    bool absolute_coordinates = false;
    int rate_min = 0;
    int rate_max = 0;
    int num_in_volley_min = 0;
    int num_in_volley_max = 0;
    int delay_between_entities_min = 0;
    int delay_between_entities_max = 0;
    bool repeat_spawns = false;
    bool dont_spawn_offscreen = false;
    bool pause_rotation_while_spawning = false;
    int time_to_pause_rotation_after_spawning = 0;
    bool spawn_if_fleeing = false;
    bool stationary_option = false;
    bool terrain_effects_option = false;
    bool set_heading = false;
    int heading_degrees = 0;
};

struct UnitStateRule {
    std::string name;
    FourCC unit_id{};
    int range = 0;
    std::string condition;
    std::string action;
};

struct UnitStateDefinition {
    std::string name;
    DefinitionFieldSet fields;
    std::vector<UnitSpawnSet> spawn_sets;
    std::vector<UnitStateRule> rules;
};

struct UnitDefinition {
    std::string name;
    std::string family_name;
    std::string description;
    DefinitionFieldSet core_fields;
    std::vector<UnitStateDefinition> states;

    [[nodiscard]] std::optional<std::size_t> find_state(std::string_view state_name) const;
};

std::optional<UnitDefinition> parse_unit_definition_document(
    const TaggedTextDocument& document,
    std::string* error = nullptr);

std::optional<UnitDefinition> decode_and_parse_unit_definition(
    std::span<const std::uint8_t> encoded,
    std::string* error = nullptr);

} // namespace deimos
