#include "deimos/sprite_resource.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace {
deimos::FourCC id(char a, char b, char c, char d) { return deimos::FourCC{{a,b,c,d}}; }
}

int main() {
    using namespace deimos;

    // Two synthetic cells using the exact plate grammar: first three bytes are
    // distinct; row 1 and the last row are separator marker; full marker
    // columns delimit cells. Cell-local border values are then trimmed.
    LegacyIndexedImage plate;
    plate.width = 13;
    plate.height = 8;
    plate.row_bytes = 13;
    plate.pixels.assign(13 * 8, 7);
    constexpr std::uint8_t marker = 9;
    plate.pixels[0] = 7;
    plate.pixels[1] = marker;
    plate.pixels[2] = 0;
    for (int x = 0; x < 13; ++x) {
        plate.pixels[1 * 13 + x] = marker;
        plate.pixels[7 * 13 + x] = marker;
    }
    for (int y = 2; y < 7; ++y) {
        for (int x : {1, 7, 12}) plate.pixels[y * 13 + x] = marker;
    }
    // Cell 1 x=2..6, y=2..6: background 0, content 3x3.
    for (int y = 2; y < 7; ++y) for (int x = 2; x < 7; ++x) plate.pixels[y*13+x] = 0;
    for (int y = 3; y <= 5; ++y) for (int x = 3; x <= 5; ++x) plate.pixels[y*13+x] = 4;
    // Cell 2 x=8..11: background 2, content 2x2 at lower right.
    for (int y = 2; y < 7; ++y) for (int x = 8; x < 12; ++x) plate.pixels[y*13+x] = 2;
    for (int y = 4; y <= 5; ++y) for (int x = 9; x <= 10; ++x) plate.pixels[y*13+x] = 5;

    std::string error;
    auto frames = extract_legacy_sprite_frames(plate, &error);
    assert(frames && error.empty());
    assert(frames->size() == 2);
    assert((*frames)[0].width == 3 && (*frames)[0].height == 3);
    assert(((*frames)[0].source_rect == LegacySpriteRect{3,3,6,6}));
    assert((*frames)[1].width == 2 && (*frames)[1].height == 2);
    assert(((*frames)[1].source_rect == LegacySpriteRect{9,4,11,6}));

    // Reconstruct the deeper 0x18D20/0x1D780 frame payload. The color key is
    // color-plate pixel (2,0); the alpha plate's red five bits are stored as
    // inverted transparency (0 opaque .. 32 transparent), with 1000 as the
    // all-transparent row sentinel.
    plate.rgb555_pixels.assign(plate.pixels.size(), 0x7fffu);
    auto color_plate = plate;
    color_plate.rgb555_pixels.assign(color_plate.pixels.size(), 0x1234u);
    color_plate.rgb555_pixels[2] = 0x03a0u;
    // Frame 0 row 0 remains white/fully transparent. Row 1 is opaque black;
    // row 2 uses a 15/31 partial-transparency grayscale.
    for (int x = 3; x < 6; ++x) {
        plate.rgb555_pixels[4 * 13 + x] = 0x0000u;
        plate.rgb555_pixels[5 * 13 + x] = static_cast<std::uint16_t>((15u << 10u) | (15u << 5u) | 15u);
    }
    auto full_group = build_legacy_sprite_group(id('t','e','s','t'), plate, color_plate, &error);
    assert(full_group && full_group->frames.size() == 2);
    const auto& surface = full_group->frames[0];
    assert(surface.transparent_key == 0x03a0u);
    assert(surface.color_pixels.size() == 9);
    assert(surface.has_surface());
    assert(surface.has_transparency_plane());
    assert(surface.transparency.size() == 9);
    assert(surface.transparency[0] == 1000u);
    assert(surface.transparency[3] == 0u);
    assert(surface.transparency[6] == 15u);

    LegacySpriteCache cache;
    LegacySpriteGroupMetadata group{id('t','e','s','t'), *frames};
    assert(cache.publish(group));
    assert(cache.group_count() == 1);
    assert(cache.frame_count(id('t','e','s','t')) == 2);
    assert(cache.find_loaded_frame(id('t','e','s','t'), 0)->width == 3);
    // 0x19AD0 high frame -> frame zero.
    assert(cache.find_loaded_frame(id('t','e','s','t'), 99)->width == 3);
    assert(cache.find_loaded_frame(id('n','o','n','e'), 0) == nullptr);

    auto d = cache.dimensions(id('t','e','s','t'), 1, 1.0f);
    assert(d.first == 2 && d.second == 2);
    d = cache.dimensions(id('t','e','s','t'), 0, 1.9f);
    assert(d.first == 5 && d.second == 5); // fctiwz/truncation

    bool loaded = false;
    d = cache.dimensions(id('l','a','z','y'), 7, 0.5f,
        [&](FourCC request, LegacySpriteCache& target) {
            assert(request == id('l','a','z','y'));
            loaded = true;
            return target.publish({request, {{LegacySpriteRect{0,0,9,7}, 9, 7}}});
        });
    assert(loaded);
    // Requested frame 7 is normalized to frame zero after lazy load.
    assert(d.first == 4 && d.second == 3);
    assert((cache.dimensions(id('n','o','n','e'), 0, 1.0f) == std::pair<int,int>{0,0}));

    // Tiny 1x1 GIF89a (single palette-index pixel). This regression binds the
    // internal GIF LZW reader without depending on external image libraries.
    const std::vector<std::uint8_t> gif = {
        'G','I','F','8','9','a', 1,0, 1,0, 0x80,0,0,
        0,0,0, 255,255,255,
        0x2c, 0,0, 0,0, 1,0, 1,0, 0,
        2, 2, 0x44,0x01, 0,
        0x3b
    };
    auto decoded = decode_legacy_gif_indices(gif, &error);
    assert(decoded);
    assert(decoded->width == 1 && decoded->height == 1);
    assert(decoded->pixels.size() == 1);
    assert(decoded->pixels[0] == 0);
    assert(decoded->rgb555_pixels.size() == 1);
    assert(decoded->rgb555_pixels[0] == 0x0000u);

    return 0;
}
