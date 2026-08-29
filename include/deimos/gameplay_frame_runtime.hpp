#pragma once

#include "deimos/presentation_runtime.hpp"
#include "deimos/score_bar_runtime.hpp"
#include "deimos/world_render_runtime.hpp"

#include <array>
#include <span>
#include <string>

namespace deimos {

// Clean-core execution boundary for the recovered normal gameplay frame.
// The original gameplay loop draws the score bar first (0x5A18 -> 0x7070 ->
// 0x31AE0), then calls the frame object (0x5AB0 -> 0x30570 -> 0x30BC0), which
// performs world composition followed by mode-1 presentation.
struct LegacyGameplayFrameScoreBarInput {
    std::array<const LegacyScoreBarPlayerState*, 2> players{{nullptr, nullptr}};
    const LegacyScoreBarConfig* config = nullptr;
    const LegacyScoreBarTextStyles* styles = nullptr;
    LegacyScoreBarRenderAssets assets{};
    // 0x30F40/0x31400 establish the static score-bar background before dirty
    // element draws. Set this when creating/resetting a presentation canvas.
    bool seed_base_panel = false;
};

struct LegacyGameplayFrameResult {
    bool score_bar_seeded = false;
    std::array<bool, 2> score_bar_rasterized{{false, false}};
    LegacyWorldRenderFrameResult world{};
    LegacyPresentationPlan presentation{};
};

// Execute the recovered visible-frame order using portable xRGB1555 surfaces:
//   1. optional static score-bar panel seed;
//   2. score-bar dirty draw for P1/P2;
//   3. 0x30BC0 world composition into the 416x480 game surface;
//   4. copy the completed game surface into source-canvas x=0..415;
//   5. mode-1 presentation plan/copy to the host display surface.
//
// Score-bar state advancement is intentionally outside this function: the
// original updater 0x317E0 runs in gameplay simulation before 0x7070 draws it.
[[nodiscard]] bool render_legacy_gameplay_frame(
    LegacyRenderQueue& queue,
    LegacyRasterSurface& game_surface,
    LegacyRasterSurface& persistent_terrain,
    const LegacyTerrainSurfaceRuntime& terrain_runtime,
    const LegacyHorizontalViewRuntime& horizontal_view,
    std::span<const LegacyParticleSystem> particle_systems,
    const LegacyGameplayFrameScoreBarInput& score_bar,
    LegacyRasterSurface& presentation_source,
    const LegacyPresentationConfig& presentation_config,
    LegacyRasterSurface& display_surface,
    bool world_draw_enabled,
    bool presentation_enabled,
    LegacyGameplayFrameResult& result,
    LegacyRasterConfig raster_config = {},
    std::string* error = nullptr);

} // namespace deimos
