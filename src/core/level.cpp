#include "deimos/level.hpp"

#include <array>

namespace deimos {
namespace {

void fail(std::string* error, std::string message) {
    if (error) *error = std::move(message);
}

bool expect_key(const TaggedRecord& record, std::string_view key, std::string* error) {
    if (record.key == key) return true;
    fail(error, "expected key '" + std::string(key) + "' at source line " +
                    std::to_string(record.source_line) + ", got '" + record.key + "'");
    return false;
}

} // namespace

std::optional<LevelDefinition> parse_level_document(const TaggedTextDocument& document, std::string* error) {
    constexpr std::array<std::string_view, 11> header_keys = {
        "name_STR", "indentifier_STR", "description_STR", "copyright_STR", "background_RECT",
        "backgroundImage_ID", "previewImage_ID", "music_ID", "mediaMask_ID", "briefing_ID", "numObjects_INT"
    };
    constexpr std::array<std::string_view, 7> object_keys = {
        "unit_ID", "layer_ID", "xLoc_INT", "yLoc_INT", "headingDegrees_INT",
        "isStationary_BOOL", "enableTerrainEffects_BOOL"
    };

    if (!document.bare_lines.empty()) {
        fail(error, "level document contains unexpected bare text");
        return std::nullopt;
    }
    if (document.records.size() < header_keys.size()) {
        fail(error, "level document is shorter than its required header");
        return std::nullopt;
    }
    for (std::size_t i = 0; i < header_keys.size(); ++i) {
        if (!expect_key(document.records[i], header_keys[i], error)) return std::nullopt;
    }

    LevelDefinition level;
    level.name = document.records[0].value;
    level.identifier = document.records[1].value;
    level.description = document.records[2].value;
    level.copyright = document.records[3].value;

    const auto background = parse_rect_value(document.records[4].value);
    const auto background_image = parse_id_value(document.records[5].value);
    const auto preview_image = parse_id_value(document.records[6].value);
    const auto music = parse_id_value(document.records[7].value);
    const auto media_mask = parse_id_value(document.records[8].value);
    const auto briefing = parse_id_value(document.records[9].value);
    const auto object_count = parse_int_value(document.records[10].value);
    if (!background || !background_image || !preview_image || !music || !media_mask || !briefing ||
        !object_count || *object_count < 0) {
        fail(error, "invalid typed value in level header");
        return std::nullopt;
    }

    level.background = *background;
    level.background_image = *background_image;
    level.preview_image = *preview_image;
    level.music = *music;
    level.media_mask = *media_mask;
    level.briefing = *briefing;

    const auto expected_records = header_keys.size() + static_cast<std::size_t>(*object_count) * object_keys.size();
    if (document.records.size() != expected_records) {
        fail(error, "declared level object count does not match record count");
        return std::nullopt;
    }

    level.objects.reserve(static_cast<std::size_t>(*object_count));
    std::size_t cursor = header_keys.size();
    for (int object_index = 0; object_index < *object_count; ++object_index) {
        for (std::size_t field = 0; field < object_keys.size(); ++field) {
            if (!expect_key(document.records[cursor + field], object_keys[field], error)) return std::nullopt;
        }
        LevelObject object;
        const auto unit = parse_id_value(document.records[cursor + 0].value);
        const auto layer = parse_id_value(document.records[cursor + 1].value);
        const auto x = parse_int_value(document.records[cursor + 2].value);
        const auto y = parse_int_value(document.records[cursor + 3].value);
        const auto heading = parse_int_value(document.records[cursor + 4].value);
        const auto stationary = parse_bool_value(document.records[cursor + 5].value);
        const auto terrain = parse_bool_value(document.records[cursor + 6].value);
        if (!unit || !layer || !x || !y || !heading || !stationary || !terrain) {
            fail(error, "invalid typed value in level object " + std::to_string(object_index));
            return std::nullopt;
        }
        object.unit_id = *unit;
        object.layer_id = *layer;
        object.x = *x;
        object.y = *y;
        object.heading_degrees = *heading;
        object.stationary = *stationary;
        object.terrain_effects = *terrain;
        level.objects.push_back(object);
        cursor += object_keys.size();
    }
    return level;
}

std::optional<LevelDefinition> decode_and_parse_level(std::span<const std::uint8_t> encoded, std::string* error) {
    const auto decoded = decode_legacy_text(encoded);
    const auto document = parse_tagged_text(decoded, error);
    if (!document) return std::nullopt;
    return parse_level_document(*document, error);
}

} // namespace deimos
