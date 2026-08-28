#include "deimos/definition_fields.hpp"

#include <cmath>
#include <limits>

namespace deimos {
namespace {

bool ends_with(std::string_view value, std::string_view suffix) {
    return value.size() >= suffix.size() && value.substr(value.size() - suffix.size()) == suffix;
}

void fail(std::string* error, const TaggedRecord& record, std::string_view message) {
    if (!error) return;
    *error = "line " + std::to_string(record.source_line) + ": " + std::string(message) +
             " for " + record.key + " <" + record.value + ">";
}

} // namespace

const DefinitionField* DefinitionFieldSet::find(std::string_view key) const {
    for (const auto& field : fields_) if (field.key == key) return &field;
    return nullptr;
}

std::optional<std::string_view> DefinitionFieldSet::string_value(std::string_view key) const {
    const auto* field = find(key);
    if (!field) return std::nullopt;
    if (const auto* value = std::get_if<std::string>(&field->value)) return *value;
    return std::nullopt;
}

std::optional<int> DefinitionFieldSet::int_value(std::string_view key) const {
    const auto* field = find(key);
    if (!field) return std::nullopt;
    if (const auto* value = std::get_if<int>(&field->value)) return *value;
    return std::nullopt;
}

std::optional<float> DefinitionFieldSet::float_value(std::string_view key) const {
    const auto* field = find(key);
    if (!field) return std::nullopt;
    if (const auto* value = std::get_if<float>(&field->value)) return *value;
    if (const auto* integer = std::get_if<int>(&field->value)) return static_cast<float>(*integer);
    return std::nullopt;
}

std::optional<bool> DefinitionFieldSet::bool_value(std::string_view key) const {
    const auto* field = find(key);
    if (!field) return std::nullopt;
    if (const auto* value = std::get_if<bool>(&field->value)) return *value;
    return std::nullopt;
}

std::optional<FourCC> DefinitionFieldSet::id_value(std::string_view key) const {
    const auto* field = find(key);
    if (!field) return std::nullopt;
    if (const auto* value = std::get_if<FourCC>(&field->value)) return *value;
    return std::nullopt;
}

std::optional<RectI> DefinitionFieldSet::rect_value(std::string_view key) const {
    const auto* field = find(key);
    if (!field) return std::nullopt;
    if (const auto* value = std::get_if<RectI>(&field->value)) return *value;
    return std::nullopt;
}

std::optional<Rgb24> DefinitionFieldSet::color_value(std::string_view key) const {
    const auto* field = find(key);
    if (!field) return std::nullopt;
    if (const auto* value = std::get_if<Rgb24>(&field->value)) return *value;
    return std::nullopt;
}

std::optional<DefinitionField> parse_definition_field(const TaggedRecord& record, std::string* error) {
    DefinitionValue value = record.value;

    if (ends_with(record.key, "_INT")) {
        if (const auto parsed = parse_int_value(record.value)) {
            value = *parsed;
        } else if (const auto parsed_float = parse_float_value(record.value)) {
            // Three canonical Player fields are historically tagged _INT yet
            // serialized as "100.000000" / "15.000000". Preserve the schema
            // contract by accepting only mathematically integral float spellings.
            const auto rounded = std::round(*parsed_float);
            if (std::fabs(*parsed_float - rounded) > 0.000001f ||
                rounded < static_cast<float>(std::numeric_limits<int>::min()) ||
                rounded > static_cast<float>(std::numeric_limits<int>::max())) {
                fail(error, record, "invalid integral _INT value");
                return std::nullopt;
            }
            value = static_cast<int>(rounded);
        } else {
            fail(error, record, "invalid _INT value");
            return std::nullopt;
        }
    } else if (ends_with(record.key, "_FLOAT")) {
        const auto parsed = parse_float_value(record.value);
        if (!parsed) { fail(error, record, "invalid _FLOAT value"); return std::nullopt; }
        value = *parsed;
    } else if (ends_with(record.key, "_BOOL")) {
        const auto parsed = parse_bool_value(record.value);
        if (!parsed) { fail(error, record, "invalid _BOOL value"); return std::nullopt; }
        value = *parsed;
    } else if (ends_with(record.key, "_ID")) {
        // Most ID fields are exact FourCC values. A few unrelated canonical
        // table fields (notably tefo/Format_ID) contain opaque tokens such as
        // "3" and "4", so a non-FourCC ID remains a string instead of being
        // rejected or padded.
        if (const auto parsed = parse_id_value(record.value)) value = *parsed;
        else value = record.value;
    } else if (ends_with(record.key, "_RECT")) {
        const auto parsed = parse_rect_value(record.value);
        if (!parsed) { fail(error, record, "invalid _RECT value"); return std::nullopt; }
        value = *parsed;
    } else if (ends_with(record.key, "_COLOR")) {
        const auto parsed = parse_rgb24_value(record.value);
        if (!parsed) { fail(error, record, "invalid _COLOR value"); return std::nullopt; }
        value = *parsed;
    }

    return DefinitionField{record.key, std::move(value), record.value, record.source_line};
}

} // namespace deimos
