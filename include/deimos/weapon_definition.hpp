#pragma once

#include "deimos/definition_fields.hpp"

#include <optional>
#include <span>
#include <string>
#include <vector>

namespace deimos {

struct WeaponSpawn {
    std::string name;
    FourCC unit_id{};
    int x = 0;
    int y = 0;
    bool set_heading = false;
    int angle_degrees = 0;
};

struct WeaponDefinition {
    std::string name;
    DefinitionFieldSet fields;
    std::vector<WeaponSpawn> spawns;
};

std::optional<WeaponDefinition> parse_weapon_definition_document(
    const TaggedTextDocument& document,
    std::string* error = nullptr);
std::optional<WeaponDefinition> decode_and_parse_weapon_definition(
    std::span<const std::uint8_t> encoded,
    std::string* error = nullptr);

} // namespace deimos
