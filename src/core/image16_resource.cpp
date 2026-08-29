#include "deimos/image16_resource.hpp"

#include <cstddef>
#include <limits>

namespace deimos {
namespace {

void fail(std::string* error, const char* message) {
    if (error) *error = message;
}

std::uint16_t le16(std::span<const std::uint8_t> bytes, std::size_t off) {
    return static_cast<std::uint16_t>(bytes[off]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[off + 1]) << 8);
}

} // namespace

std::optional<LegacyRasterSurface> decode_legacy_tga16(
    std::span<const std::uint8_t> bytes,
    std::string* error) {
    if (bytes.size() < 18) {
        fail(error, "TGA header is truncated");
        return std::nullopt;
    }

    const std::size_t id_length = bytes[0];
    const std::uint8_t color_map_type = bytes[1];
    const std::uint8_t image_type = bytes[2];
    const std::uint16_t color_map_length = le16(bytes, 5);
    const std::uint8_t color_map_depth = bytes[7];
    const int width = static_cast<int>(le16(bytes, 12));
    const int height = static_cast<int>(le16(bytes, 14));
    const std::uint8_t pixel_depth = bytes[16];
    const std::uint8_t descriptor = bytes[17];

    if (color_map_type != 0 || color_map_length != 0) {
        fail(error, "legacy TGA has an unsupported color map");
        return std::nullopt;
    }
    (void)color_map_depth; // canonical Deimos TGA headers leave this byte at 24 even with no map.
    if (image_type != 2) {
        fail(error, "legacy TGA is not uncompressed true-color type 2");
        return std::nullopt;
    }
    if (pixel_depth != 16) {
        fail(error, "legacy TGA is not 16-bit");
        return std::nullopt;
    }
    if (width <= 0 || height <= 0) {
        fail(error, "legacy TGA has invalid dimensions");
        return std::nullopt;
    }
    if (descriptor & 0x10u) {
        fail(error, "right-to-left legacy TGA origin is unsupported by the 1.0.6 contract");
        return std::nullopt;
    }

    const std::size_t pixel_offset = 18u + id_length;
    const std::size_t w = static_cast<std::size_t>(width);
    const std::size_t h = static_cast<std::size_t>(height);
    if (w > std::numeric_limits<std::size_t>::max() / h ||
        w * h > (std::numeric_limits<std::size_t>::max() - pixel_offset) / 2u) {
        fail(error, "legacy TGA dimensions overflow address space");
        return std::nullopt;
    }
    const std::size_t pixel_count = w * h;
    if (bytes.size() < pixel_offset + pixel_count * 2u) {
        fail(error, "legacy TGA pixel payload is truncated");
        return std::nullopt;
    }

    const bool top_origin = (descriptor & 0x20u) != 0;
    LegacyRasterSurface out(width, height);
    for (int file_y = 0; file_y < height; ++file_y) {
        const int dst_y = top_origin ? file_y : (height - 1 - file_y);
        for (int x = 0; x < width; ++x) {
            const std::size_t src_i = static_cast<std::size_t>(file_y) * w + static_cast<std::size_t>(x);
            const std::size_t src_off = pixel_offset + src_i * 2u;
            const std::uint16_t word = le16(bytes, src_off);
            out.pixels[static_cast<std::size_t>(dst_y) * w + static_cast<std::size_t>(x)] =
                static_cast<std::uint16_t>(word & 0x7fffu);
        }
    }
    return out;
}

} // namespace deimos
