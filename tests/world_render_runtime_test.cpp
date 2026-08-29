#include "deimos/world_render_runtime.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>

using namespace deimos;

namespace {
constexpr FourCC cost{{'C','O','S','T'}};

std::uint16_t at(const LegacyRasterSurface& s, int x, int y) {
    return s.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(s.width) +
                    static_cast<std::size_t>(x)];
}

LegacyRasterRequest rect_request(
    std::uint8_t layer, LegacyRasterRect rect, std::uint16_t color, bool terrain = false) {
    LegacyRasterRequest q;
    q.sprite_face = cost;
    q.numeric_layer = layer;
    q.special_rect = rect;
    q.clip = {0, 0, 64, 80};
    q.special_color = color;
    q.effect_amount_0_to_32 = 0;
    q.flags = terrain ? kLegacyRenderTerrainTarget : 0u;
    return q;
}

LegacyTerrainSurfaceRuntime terrain_runtime_for(LegacyRasterSurface& terrain) {
    LegacyTerrainSurfaceConfig config;
    config.visible_width = 16;
    config.visible_height = 16;
    config.display_depth = 16;
    LegacyTerrainSurfaceRuntime runtime;
    std::string error;
    assert(initialize_legacy_terrain_surface_runtime(runtime, terrain, config, &error));
    return runtime;
}

} // namespace

int main() {
    LegacyRasterSurface terrain(80, 16, 0);
    LegacyRasterSurface visible(16, 16, 0);
    auto terrain_runtime = terrain_runtime_for(terrain);
    LegacyHorizontalViewRuntime horizontal;
    LegacyRenderQueue queue;

    // Group 0 writes a persistent terrain pixel at source (33,1), which must
    // be visible at (1,1) only because 0x10120 runs after group 0.
    queue.enqueue(rect_request(1, {1, 33, 2, 34}, 0x4210, true));
    // Group 1 writes (6,7), but 0x43BA0's core pass must overwrite it.
    queue.enqueue(rect_request(2, {7, 6, 8, 7}, 0x001f));
    // Group 2 writes (7,7), proving it comes after the particle pass.
    queue.enqueue(rect_request(6, {7, 7, 8, 8}, 0x7fff));

    LegacyParticleSystem particles;
    LegacyParticle p;
    p.active = true;
    p.core_color = 0x7c00;
    p.fringe_color = 0x03e0;
    p.blend_amount = 0;
    p.x = 4.0f;
    p.y = 4.0f;
    particles.particles.push_back(p);

    LegacyWorldRenderFrameResult result;
    std::string error;
    assert(render_legacy_world_frame(
        queue, visible, terrain, terrain_runtime, horizontal,
        std::span<const LegacyParticleSystem>(&particles, 1), true, result, {}, &error));
    assert(result.terrain_viewport_copied);
    assert(result.group0_results.size() == 1);
    assert(result.group1_results.size() == 1);
    assert(result.group2_results.size() == 1);
    assert(result.particle_stats.particles_drawn == 1);
    assert(result.particle_stats.pixels_written == 49);

    assert(at(terrain, 33, 1) == 0x4210);
    assert(at(visible, 1, 1) == 0x4210); // group0 -> terrain copy
    assert(at(visible, 6, 7) == 0x7c00); // group1 -> particle core
    assert(at(visible, 7, 7) == 0x7fff); // particle -> group2

    // The caller's r28/draw latch gates only 0x10120 and 0x43BA0. All three
    // 0x18B20 group flushes still execute in the recovered 0x30BC0 ordering.
    LegacyRasterSurface terrain2(80, 16, 0);
    LegacyRasterSurface visible2(16, 16, 0);
    auto runtime2 = terrain_runtime_for(terrain2);
    LegacyRenderQueue queue2;
    queue2.enqueue(rect_request(1, {2, 34, 3, 35}, 0x4210, true));
    queue2.enqueue(rect_request(2, {3, 3, 4, 4}, 0x03e0));
    queue2.enqueue(rect_request(6, {4, 4, 5, 5}, 0x7c00));
    LegacyWorldRenderFrameResult disabled;
    assert(render_legacy_world_frame(
        queue2, visible2, terrain2, runtime2, horizontal,
        std::span<const LegacyParticleSystem>(&particles, 1), false, disabled, {}, &error));
    assert(!disabled.terrain_viewport_copied);
    assert(disabled.particle_stats.particles_drawn == 0);
    assert(at(terrain2, 34, 2) == 0x4210); // group0 still flushed
    assert(at(visible2, 2, 2) == 0);       // no terrain viewport copy
    assert(at(visible2, 3, 3) == 0x03e0);  // group1 still flushed
    assert(at(visible2, 4, 4) == 0x7c00);  // group2 still flushed

    std::cout << "world_render_runtime_test: PASS\n";
    return 0;
}
