#include "deimos/world_render_runtime.hpp"

namespace deimos {

bool render_legacy_world_frame(
    LegacyRenderQueue& queue,
    LegacyRasterSurface& visible_surface,
    LegacyRasterSurface& persistent_terrain,
    const LegacyTerrainSurfaceRuntime& terrain_runtime,
    const LegacyHorizontalViewRuntime& horizontal_view,
    std::span<const LegacyParticleSystem> particle_systems,
    bool draw_enabled,
    LegacyWorldRenderFrameResult& result,
    LegacyRasterConfig raster_config,
    std::string* error) {
    result = {};

    // 0x30CA0..0x30CA4
    result.group0_results = queue.flush_group(
        0, visible_surface, persistent_terrain, raster_config);

    // 0x30CAC..0x30CB4: r28 gates only the viewport copy.
    if (draw_enabled) {
        if (!copy_legacy_terrain_viewport(
                terrain_runtime, horizontal_view, persistent_terrain,
                visible_surface, error)) {
            return false;
        }
        result.terrain_viewport_copied = true;
    }

    // 0x30CBC..0x30CC0
    result.group1_results = queue.flush_group(
        1, visible_surface, persistent_terrain, raster_config);

    // 0x30CC8..0x30CD0: the same r28 gate wraps the particle rasterizer.
    if (draw_enabled) {
        result.particle_stats = rasterize_legacy_particles(
            particle_systems,
            visible_surface,
            terrain_runtime.config.visible_width,
            terrain_runtime.config.visible_height,
            horizontal_view.offset);
    }

    // 0x30CD8..0x30CDC
    result.group2_results = queue.flush_group(
        2, visible_surface, persistent_terrain, raster_config);
    return true;
}

} // namespace deimos
