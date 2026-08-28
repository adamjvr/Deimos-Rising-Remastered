#pragma once

#include "deimos/resource_id.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace deimos {

struct RectI {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    constexpr bool operator==(const RectI&) const = default;
};

struct Rgb24 {
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;
    constexpr bool operator==(const Rgb24&) const = default;
};

struct TaggedRecord {
    std::string key;
    std::string value;
    std::string inline_comment;
    std::size_t source_line = 0;
    std::size_t indent = 0;
};

struct BareLine {
    std::string value;
    std::size_t source_line = 0;
};

struct TaggedTextDocument {
    std::vector<TaggedRecord> records;
    std::vector<BareLine> bare_lines;
};

// Deimos Rising's legacy data files store seven-bit ASCII through a reversible
// byte transform. Two encoded bytes (bitwise complements) can represent the
// same decoded character. This function implements the observed decoder.
std::uint8_t decode_legacy_byte(std::uint8_t encoded);

// Canonical inverse used only for synthetic fixtures/tools. The original game
// data sometimes uses the complementary representation, so this is not a
// byte-for-byte reproduction of the historical encoder.
std::uint8_t encode_legacy_byte_canonical(std::uint8_t ascii7);

std::string decode_legacy_text(std::span<const std::uint8_t> encoded);
std::vector<std::uint8_t> encode_legacy_text_canonical(std::string_view text);

std::optional<TaggedTextDocument> parse_tagged_text(
    std::string_view decoded,
    std::string* error = nullptr);

std::optional<int> parse_int_value(std::string_view value);
std::optional<float> parse_float_value(std::string_view value);
std::optional<bool> parse_bool_value(std::string_view value);
std::optional<FourCC> parse_id_value(std::string_view value);
std::optional<RectI> parse_rect_value(std::string_view value);
std::optional<Rgb24> parse_rgb24_value(std::string_view value);

} // namespace deimos
