#pragma once

#include "deimos/render_backend.hpp"
#include "deimos/render_runtime.hpp"
#include "deimos/sprite_resource.hpp"

#include <cstdint>
#include <vector>

namespace deimos {

// Exact clean bridge from the semantic 0x12F20/0x12FA0/0x13460 sprite state
// into the recovered 76-byte 0x18A40/0x19570 compositor request contract.
// The caller owns screen clipping and the old immediate/queued sprite-base
// +0x35 selector because those are orchestration facts rather than state-file
// fields.
struct LegacyRenderOrchestrationContext {
    float world_x = 0.0f;
    float world_y = 0.0f;
    LegacyRasterRect clip{};
    int horizontal_view_offset = 0; // 0x100A0
    int world_y_origin = 0;         // 0xFEC0
    std::uint32_t render_sequence = 0; // 0x5CE0
    bool immediate = false;         // sprite-base +0x35 / request +0x31
};

struct LegacyRenderSubmissionBatch {
    std::vector<LegacyRasterRequest> requests;
    bool terrain_main_marked_this_sequence = false;
};

// Convert the current live semantic sprite state into the exact raw requests
// that the legacy compositor sees. Ordering is shadow first, then ordinary
// base, tint and collision-glow. A draw-to-terrain main pass is emitted only
// on the first call in a render sequence, matching sprite-base +0x90; terrain
// shadows remain independent just as 0x13460 is independent of +0x90.
[[nodiscard]] LegacyRenderSubmissionBatch build_legacy_raster_requests(
    LegacySpriteVisualRuntime& runtime,
    LegacySpriteCache& cache,
    const LegacyShadowRuntimeConfig& shadow_config,
    const LegacyRenderOrchestrationContext& context,
    LegacyRenderPassSelection selection = {},
    const LegacySpriteCache::Loader& loader = {});

// Submit a complete semantic sprite in original request order. This is a
// portable counterpart of 0x12F20 feeding 0x18A40/0x19570; it intentionally
// does not flush layer groups because the outer world renderer owns that phase.
[[nodiscard]] std::vector<LegacyRasterResult> submit_legacy_sprite_render(
    LegacySpriteVisualRuntime& runtime,
    LegacySpriteCache& cache,
    const LegacyShadowRuntimeConfig& shadow_config,
    const LegacyRenderOrchestrationContext& context,
    LegacyRenderQueue& queue,
    LegacyRasterSurface& main_surface,
    LegacyRasterSurface& terrain_surface,
    LegacyRenderPassSelection selection = {},
    LegacyRasterConfig raster_config = {},
    const LegacySpriteCache::Loader& loader = {});

// xRGB1555 conversion used by the 0x12FA0 tint/glow request fields.
[[nodiscard]] std::uint16_t legacy_rgb24_to_rgb555(Rgb24 color);

} // namespace deimos
