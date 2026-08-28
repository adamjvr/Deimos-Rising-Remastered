#pragma once

#include "deimos/legacy_text.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace deimos {

struct LevelObject {
    FourCC unit_id{};
    FourCC layer_id{};
    int x = 0;
    int y = 0;
    int heading_degrees = 0;
    bool stationary = false;
    bool terrain_effects = false;
};

struct LevelDefinition {
    std::string name;
    std::string identifier; // Original data key is misspelled "indentifier_STR".
    std::string description;
    std::string copyright;
    RectI background{};
    FourCC background_image{};
    FourCC preview_image{};
    FourCC music{};
    FourCC media_mask{};
    FourCC briefing{};
    std::vector<LevelObject> objects;
};

std::optional<LevelDefinition> parse_level_document(
    const TaggedTextDocument& document,
    std::string* error = nullptr);

std::optional<LevelDefinition> decode_and_parse_level(
    std::span<const std::uint8_t> encoded,
    std::string* error = nullptr);

} // namespace deimos
