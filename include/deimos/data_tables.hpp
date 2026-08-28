#pragma once

#include "deimos/legacy_text.hpp"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace deimos {

template <typename T>
using NamedTable = std::vector<std::pair<std::string, T>>;

std::optional<NamedTable<FourCC>> parse_id_list(
    const TaggedTextDocument& document, std::string* error = nullptr);
std::optional<NamedTable<float>> parse_float_list(
    const TaggedTextDocument& document, std::string* error = nullptr);
std::optional<NamedTable<Rgb24>> parse_color_list(
    const TaggedTextDocument& document, std::string* error = nullptr);
std::optional<NamedTable<RectI>> parse_rect_list(
    const TaggedTextDocument& document, std::string* error = nullptr);
std::optional<std::vector<std::string>> parse_string_list(
    const TaggedTextDocument& document, std::string* error = nullptr);

struct TextFormatDefinition {
    int x = 0;
    int y = 0;
    int size = 0;
    std::string format_token;
    bool monospaced = false;
    bool draw_shadows = false;
    int blend_amount_0_to_32 = 0;
    int space_between_chars = 0;
    bool colorise = false;
    Rgb24 colorise_color{};
    bool color_strip = false;
    int color_strip_h_offset = 0;
    int color_strip_v_offset = 0;
    int color_strip_blend_amount_0_to_32 = 0;
    Rgb24 color_strip_color{};
    int color_strip_min_width = 0;
    int color_strip_min_height = 0;
};

std::optional<TextFormatDefinition> parse_text_format(
    const TaggedTextDocument& document, std::string* error = nullptr);

} // namespace deimos
