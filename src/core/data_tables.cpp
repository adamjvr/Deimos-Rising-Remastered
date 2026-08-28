#include "deimos/data_tables.hpp"

#include <array>

namespace deimos {
namespace {

void fail(std::string* error, std::string message) {
    if (error) *error = std::move(message);
}

template <typename T, typename Parse>
std::optional<NamedTable<T>> parse_named_table(
    const TaggedTextDocument& document, Parse parse, std::string_view kind, std::string* error) {
    if (!document.bare_lines.empty()) {
        fail(error, std::string(kind) + " contains unexpected bare lines");
        return std::nullopt;
    }
    NamedTable<T> result;
    result.reserve(document.records.size());
    for (const auto& record : document.records) {
        auto value = parse(record.value);
        if (!value) {
            fail(error, "invalid " + std::string(kind) + " value for key '" + record.key +
                "' at source line " + std::to_string(record.source_line));
            return std::nullopt;
        }
        result.emplace_back(record.key, *value);
    }
    return result;
}

bool key(const TaggedRecord& r, std::string_view expected, std::string* error) {
    if (r.key == expected) return true;
    fail(error, "expected text-format key '" + std::string(expected) + "', got '" + r.key + "'");
    return false;
}

} // namespace

std::optional<NamedTable<FourCC>> parse_id_list(const TaggedTextDocument& document, std::string* error) {
    return parse_named_table<FourCC>(document, parse_id_value, "ID-list", error);
}
std::optional<NamedTable<float>> parse_float_list(const TaggedTextDocument& document, std::string* error) {
    return parse_named_table<float>(document, parse_float_value, "float-list", error);
}
std::optional<NamedTable<Rgb24>> parse_color_list(const TaggedTextDocument& document, std::string* error) {
    return parse_named_table<Rgb24>(document, parse_rgb24_value, "color-list", error);
}
std::optional<NamedTable<RectI>> parse_rect_list(const TaggedTextDocument& document, std::string* error) {
    return parse_named_table<RectI>(document, parse_rect_value, "rect-list", error);
}

std::optional<std::vector<std::string>> parse_string_list(const TaggedTextDocument& document, std::string* error) {
    if (!document.records.empty()) {
        fail(error, "string-list contains tagged records");
        return std::nullopt;
    }
    std::vector<std::string> result;
    result.reserve(document.bare_lines.size());
    for (const auto& line : document.bare_lines) result.push_back(line.value);
    return result;
}

std::optional<TextFormatDefinition> parse_text_format(const TaggedTextDocument& document, std::string* error) {
    constexpr std::array<std::string_view, 17> keys = {
        "Loc_X_INT", "Loc_Y_INT", "Size_INT", "Format_ID", "Monospaced_BOOL", "DrawShadows_BOOL",
        "BlendAmount_0To32_INT", "SpaceBetweenChars_INT", "Colorise_Do_BOOL", "ColoriseColor_RGB",
        "ColorStrip_Do_BOOL", "ColorStrip_HOffset_INT", "ColorStrip_VOffset_INT",
        "ColorStrip_BlendAmount_0To32_INT", "ColorStrip_Color_RGB", "ColorStrip_MinWidth_INT",
        "ColorStrip_MinHeight_INT"
    };
    if (!document.bare_lines.empty() || document.records.size() != keys.size()) {
        fail(error, "text-format must contain exactly 17 tagged records");
        return std::nullopt;
    }
    for (std::size_t i = 0; i < keys.size(); ++i) if (!key(document.records[i], keys[i], error)) return std::nullopt;

    TextFormatDefinition out;
    auto x=parse_int_value(document.records[0].value); auto y=parse_int_value(document.records[1].value);
    auto size=parse_int_value(document.records[2].value); const auto& format=document.records[3].value;
    auto mono=parse_bool_value(document.records[4].value); auto shadows=parse_bool_value(document.records[5].value);
    auto blend=parse_int_value(document.records[6].value); auto spacing=parse_int_value(document.records[7].value);
    auto colorise=parse_bool_value(document.records[8].value); auto color=parse_rgb24_value(document.records[9].value);
    auto strip=parse_bool_value(document.records[10].value); auto hx=parse_int_value(document.records[11].value);
    auto vy=parse_int_value(document.records[12].value); auto sblend=parse_int_value(document.records[13].value);
    auto scolor=parse_rgb24_value(document.records[14].value); auto minw=parse_int_value(document.records[15].value);
    auto minh=parse_int_value(document.records[16].value);
    if (!x||!y||!size||format.empty()||!mono||!shadows||!blend||!spacing||!colorise||!color||!strip||!hx||!vy||!sblend||!scolor||!minw||!minh) {
        fail(error, "invalid typed value in text-format resource");
        return std::nullopt;
    }
    out.x=*x; out.y=*y; out.size=*size; out.format_token=format; out.monospaced=*mono; out.draw_shadows=*shadows;
    out.blend_amount_0_to_32=*blend; out.space_between_chars=*spacing; out.colorise=*colorise; out.colorise_color=*color;
    out.color_strip=*strip; out.color_strip_h_offset=*hx; out.color_strip_v_offset=*vy;
    out.color_strip_blend_amount_0_to_32=*sblend; out.color_strip_color=*scolor;
    out.color_strip_min_width=*minw; out.color_strip_min_height=*minh;
    return out;
}

} // namespace deimos
