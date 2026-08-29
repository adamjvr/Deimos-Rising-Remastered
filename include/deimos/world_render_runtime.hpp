#pragma once

#include "deimos/particle_runtime.hpp"
#include "deimos/render_backend.hpp"
#include "deimos/terrain_runtime.hpp"

#include <span>
#include <string>
#include <vector>

namespace deimos {

// The recovered outer gameplay-frame composition boundary around PPC 0x30BC0.
// 0x18B20 groups are unconditional; the caller's draw-enabled latch (r28 in
// the original) gates only 0x10120 terrain viewport copy and 0x43BA0 particles.
struct LegacyWorldRenderFrameResult {
    std::vector<LegacyRasterResult> group0_results; // layers 0..1
    bool terrain_viewport_copied = false;
    std::vector<LegacyRasterResult> group1_results; // layers 2..5
    LegacyParticleRasterStats particle_stats{};
    std::vector<LegacyRasterResult> group2_results; // layers 6..15
};

// Executes the exact recovered ordering:
//   0x18B20(0) -> [0x10120] -> 0x18B20(1) -> [0x43BA0] -> 0x18B20(2)
// Bracketed passes run only when draw_enabled is true. This function owns the
// recovered world-composition order but deliberately stops before native
// DrawSprocket/window presentation, which remains a separate platform layer.
[[nodiscard]] bool render_legacy_world_frame(
    LegacyRenderQueue& queue,
    LegacyRasterSurface& visible_surface,
    LegacyRasterSurface& persistent_terrain,
    const LegacyTerrainSurfaceRuntime& terrain_runtime,
    const LegacyHorizontalViewRuntime& horizontal_view,
    std::span<const LegacyParticleSystem> particle_systems,
    bool draw_enabled,
    LegacyWorldRenderFrameResult& result,
    LegacyRasterConfig raster_config = {},
    std::string* error = nullptr);

} // namespace deimos
