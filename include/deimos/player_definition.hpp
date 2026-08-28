#pragma once

#include "deimos/definition_fields.hpp"

#include <optional>
#include <span>
#include <string>

namespace deimos {

struct PlayerDefinition {
    std::string name;
    DefinitionFieldSet fields;
};

std::optional<PlayerDefinition> parse_player_definition_document(
    const TaggedTextDocument& document,
    std::string* error = nullptr);
std::optional<PlayerDefinition> decode_and_parse_player_definition(
    std::span<const std::uint8_t> encoded,
    std::string* error = nullptr);

} // namespace deimos
