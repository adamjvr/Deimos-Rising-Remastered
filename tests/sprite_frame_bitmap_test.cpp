#include "deimos/sprite_resource.hpp"

#include <cassert>
#include <cstdint>
#include <string>

namespace {
deimos::FourCC id(const char* s) { return deimos::FourCC{{s[0],s[1],s[2],s[3]}}; }

deimos::LegacyIndexedImage one_cell_plate() {
    deimos::LegacyIndexedImage image;
    image.width = 7;
    image.height = 7;
    image.row_bytes = 7;
    image.pixels.assign(49, 0);
    constexpr std::uint8_t marker = 9;
    image.pixels[0] = 7;
    image.pixels[1] = marker;
    image.pixels[2] = 0;
    for (int x = 0; x < 7; ++x) {
        image.pixels[7 + x] = marker;
        image.pixels[6 * 7 + x] = marker;
    }
    for (int y = 2; y < 6; ++y) {
        image.pixels[y * 7 + 1] = marker;
        image.pixels[y * 7 + 6] = marker;
    }
    // 5x4 cell, trimmed down to a 3x2 frame at x=3..5, y=3..4.
    for (int y = 2; y < 6; ++y) for (int x = 2; x < 6; ++x) image.pixels[y*7+x] = 0;
    for (int y = 3; y <= 4; ++y) for (int x = 3; x <= 5; ++x) image.pixels[y*7+x] = 4;
    image.rgb555_pixels.assign(49, 0x7fffu); // red5=31 -> full transparency (32)
    return image;
}
}

int main() {
    using namespace deimos;
    std::string error;

    auto alpha = one_cell_plate();
    auto color = alpha;
    color.rgb555_pixels.assign(49, 0x1234u);
    color.rgb555_pixels[2] = 0x03a0u;

    // 0x1EEC0 plane: first row opaque, second row partially transparent.
    for (int x = 3; x <= 5; ++x) alpha.rgb555_pixels[3 * 7 + x] = 0x0000u;
    for (int x = 3; x <= 5; ++x) {
        alpha.rgb555_pixels[4 * 7 + x] = static_cast<std::uint16_t>((15u << 10u) | (15u << 5u) | 15u);
    }
    auto group = build_legacy_sprite_group(id("surf"), alpha, color, &error);
    assert(group && error.empty());
    assert(group->frames.size() == 1);
    const auto& frame = group->frames.front();
    assert(frame.width == 3 && frame.height == 2);
    assert(frame.transparent_key == 0x03a0u);
    assert(frame.color_pixels.size() == 6);
    assert(frame.has_surface());
    assert(frame.has_transparency_plane());
    assert(frame.transparency.size() == 6);
    assert(frame.transparency[0] == 0u);
    assert(frame.transparency[1] == 0u);
    assert(frame.transparency[3] == 15u);

    // If the alpha crop has no value below red5=31, 0x1EEC0 frees the whole
    // secondary plane. The 0x1D780 object then falls back to its xRGB1555
    // transparent-key comparison path.
    auto key_only_alpha = one_cell_plate();
    auto key_only = build_legacy_sprite_group(id("key "), key_only_alpha, color, &error);
    assert(key_only && key_only->frames.size() == 1);
    assert(key_only->frames[0].has_surface());
    assert(!key_only->frames[0].has_transparency_plane());
    assert(key_only->frames[0].transparent_key == 0x03a0u);

    // Fully transparent rows in an allocated plane store 1000 in slot zero.
    auto row_alpha = one_cell_plate();
    // Allocate the plane from one opaque pixel on row 1 only; row 0 remains
    // red5=31 and must receive the fast-skip sentinel.
    row_alpha.rgb555_pixels[4 * 7 + 3] = 0x0000u;
    auto row_group = build_legacy_sprite_group(id("rows"), row_alpha, color, &error);
    assert(row_group && row_group->frames[0].has_transparency_plane());
    assert(row_group->frames[0].transparency[0] == 1000u);
    assert(row_group->frames[0].transparency[3] == 0u);

    return 0;
}
