#include "deimos/legacy_text.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <limits>

namespace deimos {
namespace {

std::string_view trim(std::string_view value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
        value.remove_prefix(1);
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
        value.remove_suffix(1);
    }
    return value;
}

void set_error(std::string* error, std::size_t line, std::string_view message) {
    if (!error) return;
    *error = "line " + std::to_string(line) + ": " + std::string(message);
}

} // namespace

std::uint8_t decode_legacy_byte(std::uint8_t encoded) {
    std::uint8_t decoded = static_cast<std::uint8_t>(((encoded & 0x07u) << 4u) | (encoded >> 4u));
    if ((encoded & 0x08u) != 0) decoded ^= 0x7fu;
    return decoded;
}

std::uint8_t encode_legacy_byte_canonical(std::uint8_t ascii7) {
    ascii7 &= 0x7fu;
    return static_cast<std::uint8_t>(((ascii7 & 0x0fu) << 4u) | ((ascii7 >> 4u) & 0x07u));
}

std::string decode_legacy_text(std::span<const std::uint8_t> encoded) {
    std::string output;
    output.reserve(encoded.size());
    for (const auto byte : encoded) {
        output.push_back(static_cast<char>(decode_legacy_byte(byte)));
    }
    // Several canonical resources carry a transformed trailing NUL. Preserve
    // any interior byte exactly and trim only terminal padding.
    while (!output.empty() && output.back() == '\0') output.pop_back();
    return output;
}

std::vector<std::uint8_t> encode_legacy_text_canonical(std::string_view text) {
    std::vector<std::uint8_t> output;
    output.reserve(text.size());
    for (const unsigned char c : text) output.push_back(encode_legacy_byte_canonical(c));
    return output;
}

std::optional<TaggedTextDocument> parse_tagged_text(std::string_view decoded, std::string* error) {
    TaggedTextDocument document;
    std::size_t line_number = 0;
    std::size_t cursor = 0;

    while (cursor <= decoded.size()) {
        const auto end = decoded.find_first_of("\r\n", cursor);
        auto line = decoded.substr(cursor, end == std::string_view::npos ? decoded.size() - cursor : end - cursor);
        ++line_number;

        if (end == std::string_view::npos) {
            cursor = decoded.size() + 1;
        } else {
            cursor = end + 1;
            if (decoded[end] == '\r' && cursor < decoded.size() && decoded[cursor] == '\n') ++cursor;
        }

        std::size_t indent = 0;
        while (indent < line.size() && (line[indent] == ' ' || line[indent] == '\t')) ++indent;
        auto content = line.substr(indent);
        auto stripped = trim(content);
        if (stripped.empty() || stripped.starts_with("//")) continue;

        if (!stripped.starts_with('#')) {
            document.bare_lines.push_back({std::string(stripped), line_number});
            continue;
        }

        const auto open = stripped.find('<');
        if (open == std::string_view::npos) {
            set_error(error, line_number, "tagged record has no '<' value marker");
            return std::nullopt;
        }
        const auto close = stripped.find('>', open + 1);
        if (close == std::string_view::npos) {
            set_error(error, line_number, "tagged record has no '>' value marker");
            return std::nullopt;
        }

        const auto key = trim(stripped.substr(1, open - 1));
        if (key.empty()) {
            set_error(error, line_number, "empty tag key");
            return std::nullopt;
        }

        auto suffix = trim(stripped.substr(close + 1));
        std::string inline_comment;
        if (!suffix.empty()) {
            if (!suffix.starts_with("//")) {
                set_error(error, line_number, "unexpected text after tagged value");
                return std::nullopt;
            }
            inline_comment = std::string(trim(suffix.substr(2)));
        }

        document.records.push_back({
            std::string(key), std::string(stripped.substr(open + 1, close - open - 1)),
            std::move(inline_comment), line_number, indent
        });
    }

    return document;
}

std::optional<int> parse_int_value(std::string_view value) {
    value = trim(value);
    int result = 0;
    const auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), result);
    if (ec != std::errc{} || ptr != value.data() + value.size()) return std::nullopt;
    return result;
}

std::optional<float> parse_float_value(std::string_view value) {
    value = trim(value);
    if (value.empty()) return std::nullopt;
    std::string copy(value);
    char* end = nullptr;
    errno = 0;
    const float result = std::strtof(copy.c_str(), &end);
    if (errno == ERANGE || end != copy.c_str() + copy.size()) return std::nullopt;
    return result;
}

std::optional<bool> parse_bool_value(std::string_view value) {
    value = trim(value);
    if (value == "TRUE") return true;
    if (value == "FALSE") return false;
    return std::nullopt;
}

std::optional<FourCC> parse_id_value(std::string_view value) {
    // FourCC values are fixed-width identifiers. Whitespace can be semantic
    // (for example the canonical layer ID "air "), so never trim it.
    if (value.size() != 4) return std::nullopt;
    FourCC result;
    std::copy_n(value.begin(), 4, result.bytes.begin());
    return result;
}

std::optional<RectI> parse_rect_value(std::string_view value) {
    RectI result;
    int* fields[] = {&result.left, &result.top, &result.right, &result.bottom};
    for (std::size_t i = 0; i < 4; ++i) {
        const auto comma = value.find(',');
        auto token = trim(value.substr(0, comma));
        const auto parsed = parse_int_value(token);
        if (!parsed) return std::nullopt;
        *fields[i] = *parsed;
        if (i < 3) {
            if (comma == std::string_view::npos) return std::nullopt;
            value.remove_prefix(comma + 1);
        } else if (comma != std::string_view::npos) {
            return std::nullopt;
        }
    }
    return result;
}

std::optional<Rgb24> parse_rgb24_value(std::string_view value) {
    value = trim(value);
    if (value.size() != 6) return std::nullopt;
    unsigned result = 0;
    const auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), result, 16);
    if (ec != std::errc{} || ptr != value.data() + value.size() || result > 0xffffffu) return std::nullopt;
    return Rgb24{
        static_cast<std::uint8_t>((result >> 16u) & 0xffu),
        static_cast<std::uint8_t>((result >> 8u) & 0xffu),
        static_cast<std::uint8_t>(result & 0xffu)
    };
}

} // namespace deimos
