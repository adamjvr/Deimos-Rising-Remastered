#include "deimos/film.hpp"

#include <algorithm>

namespace deimos {
namespace {

std::uint32_t be32(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return (static_cast<std::uint32_t>(bytes[offset]) << 24u) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 16u) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 8u) |
            static_cast<std::uint32_t>(bytes[offset + 3]);
}

FourCC fourcc(std::span<const std::uint8_t> bytes, std::size_t offset) {
    FourCC result;
    for (std::size_t i = 0; i < 4; ++i) result.bytes[i] = static_cast<char>(bytes[offset + i]);
    return result;
}

void fail(std::string* error, std::string message) {
    if (error) *error = std::move(message);
}

} // namespace

std::optional<FilmV10005> parse_film_v10005(std::span<const std::uint8_t> bytes, std::string* error) {
    if (bytes.size() != FilmV10005::file_size) {
        fail(error, "film v10005 must be exactly 40296 bytes");
        return std::nullopt;
    }

    FilmV10005 film;
    film.header.format_version = be32(bytes, 0x00);
    if (film.header.format_version != 10005u) {
        fail(error, "unsupported film format version " + std::to_string(film.header.format_version));
        return std::nullopt;
    }
    film.header.field_04 = be32(bytes, 0x04);
    film.header.level_id_a = fourcc(bytes, 0x08);
    film.header.mode_byte = bytes[0x0c];
    film.header.recorded_ticks = be32(bytes, 0x10);
    film.header.field_14 = be32(bytes, 0x14);
    film.header.level_id_b = fourcc(bytes, 0x18);

    if (film.header.recorded_ticks > FilmV10005::primary_input_capacity) {
        fail(error, "film declares more than 20000 recorded input ticks");
        return std::nullopt;
    }

    const auto input_begin = bytes.begin() + static_cast<std::ptrdiff_t>(FilmV10005::header_size);
    film.primary_input.assign(input_begin,
        input_begin + static_cast<std::ptrdiff_t>(film.header.recorded_ticks));

    // All current v10005 evidence is seven-bit. Fail closed instead of silently
    // inventing semantics for a future high-bit flag.
    if (std::any_of(film.primary_input.begin(), film.primary_input.end(), [](std::uint8_t mask) {
            return (mask & 0x80u) != 0;
        })) {
        fail(error, "film contains an unknown high input-mask bit");
        return std::nullopt;
    }

    std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(FilmV10005::header_size +
                    FilmV10005::primary_input_capacity),
                film.primary_tail.size(), film.primary_tail.begin());
    return film;
}

} // namespace deimos
