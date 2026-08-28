#include "deimos/weapon_definition.hpp"

namespace deimos {
namespace {
void fail(std::string* error, std::string message) { if (error) *error = std::move(message); }

std::optional<int> as_int(const TaggedRecord& r, std::string* error) {
    if (auto v = parse_int_value(r.value)) return v;
    fail(error, "line " + std::to_string(r.source_line) + ": invalid integer for " + r.key);
    return std::nullopt;
}
std::optional<bool> as_bool(const TaggedRecord& r, std::string* error) {
    if (auto v = parse_bool_value(r.value)) return v;
    fail(error, "line " + std::to_string(r.source_line) + ": invalid Boolean for " + r.key);
    return std::nullopt;
}
std::optional<FourCC> as_id(const TaggedRecord& r, std::string* error) {
    if (auto v = parse_id_value(r.value)) return v;
    fail(error, "line " + std::to_string(r.source_line) + ": invalid FourCC for " + r.key);
    return std::nullopt;
}
}

std::optional<WeaponDefinition> parse_weapon_definition_document(
    const TaggedTextDocument& document, std::string* error) {
    if (!document.bare_lines.empty()) { fail(error, "weapon definition contains bare text"); return std::nullopt; }
    WeaponDefinition out;
    std::size_t pos = 0;
    while (pos < document.records.size() && document.records[pos].key != "spawn_NumUnitsToSpawn_INT") {
        auto field = parse_definition_field(document.records[pos], error);
        if (!field) return std::nullopt;
        out.fields.add(std::move(*field));
        ++pos;
    }
    if (pos >= document.records.size()) { fail(error, "weapon definition missing spawn count"); return std::nullopt; }
    const auto spawn_count = as_int(document.records[pos++], error);
    if (!spawn_count || *spawn_count < 0) return std::nullopt;
    out.spawns.reserve(static_cast<std::size_t>(*spawn_count));

    static constexpr const char* keys[] = {
        "spawn_Name_STR", "spawn_Unit_ID", "spawn_XLoc_INT", "spawn_YLoc_INT",
        "spawn_SetHeading_BOOL", "spawn_Angle_INT"
    };
    for (int i = 0; i < *spawn_count; ++i) {
        const TaggedRecord* r[6]{};
        for (std::size_t k = 0; k < 6; ++k) {
            if (pos >= document.records.size() || document.records[pos].key != keys[k]) {
                fail(error, "weapon spawn record schema mismatch at spawn " + std::to_string(i));
                return std::nullopt;
            }
            r[k] = &document.records[pos++];
        }
        auto id = as_id(*r[1], error); auto x = as_int(*r[2], error); auto y = as_int(*r[3], error);
        auto heading = as_bool(*r[4], error); auto angle = as_int(*r[5], error);
        if (!id || !x || !y || !heading || !angle) return std::nullopt;
        out.spawns.push_back({r[0]->value, *id, *x, *y, *heading, *angle});
    }
    while (pos < document.records.size()) {
        auto field = parse_definition_field(document.records[pos++], error);
        if (!field) return std::nullopt;
        out.fields.add(std::move(*field));
    }
    const auto name = out.fields.string_value("name_STR");
    if (!name) { fail(error, "weapon definition missing name_STR"); return std::nullopt; }
    out.name = std::string(*name);
    return out;
}

std::optional<WeaponDefinition> decode_and_parse_weapon_definition(
    std::span<const std::uint8_t> encoded, std::string* error) {
    auto doc = parse_tagged_text(decode_legacy_text(encoded), error);
    if (!doc) return std::nullopt;
    return parse_weapon_definition_document(*doc, error);
}

} // namespace deimos
