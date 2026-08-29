#include "deimos/terrain_runtime.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

using namespace deimos;

namespace {

std::uint16_t sample_pixel(int x, int y) {
    return static_cast<std::uint16_t>(((y * 97) + (x * 13)) & 0x7fff);
}

LegacyRasterSurface make_terrain(int width, int height) {
    LegacyRasterSurface surface(width, height, 0);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            surface.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                           static_cast<std::size_t>(x)] = sample_pixel(x, y);
        }
    }
    return surface;
}

} // namespace

int main() {
    // Game[gafl] 54..56 is the positional configuration consumed directly by
    // 0x10120/0x10220 and by 0xFBC0's 16-bit persistent-surface allocation.
    NamedTable<float> game(57, {"unused", 0.0f});
    game[54] = {"VisibleGameWidth", 416.0f};
    game[55] = {"VisibleGameHeight", 480.0f};
    game[56] = {"ReqDisplayDepth", 16.0f};
    std::string error;
    const auto config = compile_legacy_terrain_surface_config(game, &error);
    assert(config);
    assert(config->visible_width == 416);
    assert(config->visible_height == 480);
    assert(config->display_depth == 16);
    assert(config->horizontal_source_bias == 32);
    assert(config->row_activation_margin == 64);

    game[55].first = "wrong";
    assert(!compile_legacy_terrain_surface_config(game, &error));
    game[55] = {"VisibleGameHeight", 480.0f};

    auto terrain = make_terrain(480, 1000);
    LegacyRasterSurface visible(640, 480, 0xffff);
    LegacyTerrainSurfaceRuntime runtime;
    assert(initialize_legacy_terrain_surface_runtime(runtime, terrain, *config, &error));

    // 0xFA90 starts at the bottom-most 416x480 crop, offset 32 pixels into
    // the 480-wide source, and seeds vertical progress to 480+1.
    assert((runtime.full_bounds == LegacyRasterRect{0, 0, 1000, 480}));
    assert((runtime.source_view == LegacyRasterRect{520, 32, 1000, 448}));
    assert(runtime.requested_vertical_delta == 1);
    assert(runtime.applied_vertical_delta == 0);
    assert(runtime.vertical_progress == 481);
    assert(!runtime.reached_end);

    // 0xFA10's loop is height+65 calls: bottom through top-64 inclusive.
    std::vector<int> primed_rows;
    prime_legacy_terrain_rows(runtime, [&](int y) { primed_rows.push_back(y); });
    assert(primed_rows.size() == 545);
    assert(primed_rows.front() == 1000);
    assert(primed_rows.back() == 456);
    auto suppressed = runtime;
    suppressed.row_updates_suppressed = true;
    int suppressed_calls = 0;
    prime_legacy_terrain_rows(suppressed, [&](int) { ++suppressed_calls; });
    assert(suppressed_calls == 0);

    // 0x10120 copies the ENTIRE source viewport every time. Mutations to the
    // persistent terrain therefore survive frame-to-frame and reappear in the
    // visible surface without any strip/dirty-region backing store.
    LegacyHorizontalViewRuntime horizontal;
    assert(copy_legacy_terrain_viewport(runtime, horizontal, terrain, visible, &error));
    assert(visible.pixels[0] == sample_pixel(32, 520));
    assert(visible.pixels[479u * 640u + 415u] == sample_pixel(447, 999));

    const std::size_t persistent_stamp = 700u * 480u + 100u;
    terrain.pixels[persistent_stamp] = 0x1234;
    assert(copy_legacy_terrain_viewport(runtime, horizontal, terrain, visible, &error));
    assert(visible.pixels[180u * 640u + 68u] == 0x1234);

    horizontal.offset = -32;
    assert(copy_legacy_terrain_viewport(runtime, horizontal, terrain, visible, &error));
    assert(visible.pixels[0] == sample_pixel(0, 520));
    horizontal.offset = 31;
    assert(copy_legacy_terrain_viewport(runtime, horizontal, terrain, visible, &error));
    assert(visible.pixels[0] == sample_pixel(63, 520));
    assert(visible.pixels[415] == sample_pixel(478, 520));

    // 0x10220 scrolls the source rect only. One +1 request moves the camera
    // upward by one source pixel and reports exactly +1 as the applied delta.
    runtime.requested_vertical_delta = 1;
    step_legacy_vertical_terrain_view(runtime, terrain);
    assert(runtime.source_view.top == 519);
    assert(runtime.source_view.bottom == 999);
    assert(runtime.vertical_progress == 482);
    assert(runtime.applied_vertical_delta == 1);

    // 0x10000 then activates exactly one row 64 pixels above the new top.
    int activated_row = 0;
    int activation_calls = 0;
    assert(!tick_legacy_terrain_scroll(runtime, terrain, [&](int y) {
        ++activation_calls;
        activated_row = y;
    }));
    assert(runtime.source_view.top == 518);
    assert(runtime.source_view.bottom == 998);
    assert(runtime.applied_vertical_delta == 1);
    assert(activation_calls == 1);
    assert(activated_row == 454);

    // A zero request performs no movement and clears the applied delta.
    runtime.requested_vertical_delta = 0;
    assert(!tick_legacy_terrain_scroll(runtime, terrain));
    assert(runtime.applied_vertical_delta == 0);

    // Normal +1 scrolling ends at source top==1, not 0: vertical_progress was
    // seeded to 481, so 0x10000 latches at full height one tick before 0x10220
    // would reach the top edge. Preserve this executable one-pixel quirk.
    auto short_terrain = make_terrain(480, 490);
    LegacyTerrainSurfaceRuntime ending;
    assert(initialize_legacy_terrain_surface_runtime(ending, short_terrain, *config, &error));
    int normal_activation_calls = 0;
    bool done = false;
    for (int i = 0; i < 9; ++i) {
        done = tick_legacy_terrain_scroll(ending, short_terrain, [&](int) {
            ++normal_activation_calls;
        });
    }
    assert(done);
    assert(ending.reached_end);
    assert(ending.requested_vertical_delta == 0);
    assert(ending.vertical_progress == 490);
    assert(ending.source_view.top == 1);
    assert(ending.source_view.bottom == 481);
    assert(normal_activation_calls == 8); // end-latching tick skips 0x33090
    assert(tick_legacy_terrain_scroll(ending, short_terrain));
    assert(ending.applied_vertical_delta == 0);

    // Direct 0x10220 top clamp uses <=0 and reconstructs a 480-high view.
    LegacyTerrainSurfaceRuntime top_clamp;
    assert(initialize_legacy_terrain_surface_runtime(top_clamp, terrain, *config, &error));
    top_clamp.requested_vertical_delta = 600;
    step_legacy_vertical_terrain_view(top_clamp, terrain);
    assert(top_clamp.source_view.top == 0);
    assert(top_clamp.source_view.bottom == 480);
    assert(top_clamp.applied_vertical_delta == 520);
    assert(top_clamp.vertical_progress == 1000);

    // Reverse/debug movement can run past the lower edge, where 0x10220 clamps
    // back to the terrain bottom and therefore reports zero applied movement.
    LegacyTerrainSurfaceRuntime bottom_clamp;
    assert(initialize_legacy_terrain_surface_runtime(bottom_clamp, terrain, *config, &error));
    bottom_clamp.requested_vertical_delta = -20;
    step_legacy_vertical_terrain_view(bottom_clamp, terrain);
    assert(bottom_clamp.source_view.top == 520);
    assert(bottom_clamp.source_view.bottom == 1000);
    assert(bottom_clamp.applied_vertical_delta == 0);
    assert(bottom_clamp.vertical_progress == 461);

    LegacyRasterSurface too_small(415, 480, 0);
    assert(!copy_legacy_terrain_viewport(ending, LegacyHorizontalViewRuntime{}, short_terrain,
                                         too_small, &error));

    std::cout << "terrain_surface_runtime_test: PASS\n";
    return 0;
}
