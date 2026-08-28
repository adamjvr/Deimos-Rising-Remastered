#pragma once

#include "deimos/legacy_text.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace deimos {

using DefinitionValue = std::variant<std::string, int, float, bool, FourCC, RectI, Rgb24>;

struct DefinitionField {
    std::string key;
    DefinitionValue value;
    std::string raw_value;
    std::size_t source_line = 0;
};

class DefinitionFieldSet {
public:
    void add(DefinitionField field) { fields_.push_back(std::move(field)); }
    [[nodiscard]] const std::vector<DefinitionField>& fields() const { return fields_; }
    [[nodiscard]] const DefinitionField* find(std::string_view key) const;
    [[nodiscard]] bool contains(std::string_view key) const { return find(key) != nullptr; }

    [[nodiscard]] std::optional<std::string_view> string_value(std::string_view key) const;
    [[nodiscard]] std::optional<int> int_value(std::string_view key) const;
    [[nodiscard]] std::optional<float> float_value(std::string_view key) const;
    [[nodiscard]] std::optional<bool> bool_value(std::string_view key) const;
    [[nodiscard]] std::optional<FourCC> id_value(std::string_view key) const;
    [[nodiscard]] std::optional<RectI> rect_value(std::string_view key) const;
    [[nodiscard]] std::optional<Rgb24> color_value(std::string_view key) const;

private:
    std::vector<DefinitionField> fields_;
};

std::optional<DefinitionField> parse_definition_field(
    const TaggedRecord& record,
    std::string* error = nullptr);

} // namespace deimos
