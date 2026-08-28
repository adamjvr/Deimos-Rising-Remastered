#include "deimos/player_definition.hpp"

namespace deimos {
namespace {
void fail(std::string* error, std::string message) { if (error) *error = std::move(message); }
}

std::optional<PlayerDefinition> parse_player_definition_document(
    const TaggedTextDocument& document, std::string* error) {
    if (!document.bare_lines.empty()) { fail(error, "player definition contains bare text"); return std::nullopt; }
    PlayerDefinition out;
    for (const auto& record : document.records) {
        auto field = parse_definition_field(record, error);
        if (!field) return std::nullopt;
        out.fields.add(std::move(*field));
    }
    const auto name = out.fields.string_value("name_STR");
    if (!name) { fail(error, "player definition missing name_STR"); return std::nullopt; }
    out.name = std::string(*name);
    return out;
}

std::optional<PlayerDefinition> decode_and_parse_player_definition(
    std::span<const std::uint8_t> encoded, std::string* error) {
    auto doc = parse_tagged_text(decode_legacy_text(encoded), error);
    if (!doc) return std::nullopt;
    return parse_player_definition_document(*doc, error);
}

} // namespace deimos
