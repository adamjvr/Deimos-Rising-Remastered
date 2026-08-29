#include "deimos/gameplay_frame_runtime.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <span>
#include <string>

using namespace deimos;

namespace {
std::uint16_t at(const LegacyRasterSurface& s, int x, int y) {
    return s.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(s.width) +
                    static_cast<std::size_t>(x)];
}

LegacyPresentationConfig presentation_config() {
    LegacyPresentationConfig c;
    c.min_screen_width = 640;
    c.min_screen_height = 480;
    c.visible_game_width = 416;
    c.visible_game_height = 480;
    c.display_depth = 16;
    c.score_bar_width = 160;
    c.score_bar_height = 480;
    c.left_border_width = 32;
    c.right_border_width = 32;
    return c;
}
}

int main() {
    std::string error;

    LegacyTerrainSurfaceConfig terrain_cfg;
    terrain_cfg.visible_width = 416;
    terrain_cfg.visible_height = 480;
    terrain_cfg.display_depth = 16;

    // The source view begins at x=32, so a 480-wide terrain surface safely
    // covers the canonical 32+416 horizontal extent.
    LegacyRasterSurface terrain(480, 480, 0x2222);
    LegacyTerrainSurfaceRuntime terrain_runtime;
    assert(initialize_legacy_terrain_surface_runtime(
        terrain_runtime, terrain, terrain_cfg, &error));

    LegacyHorizontalViewRuntime horizontal;
    LegacyRasterSurface game(416, 480, 0);
    LegacyRasterSurface source(576, 480, 0x1111);
    LegacyRasterSurface display(640, 480, 0x7777);
    LegacyRasterSurface panel(160, 480, 0x3333);
    LegacyRenderQueue queue;

    LegacyGameplayFrameScoreBarInput score_bar;
    score_bar.assets.base_panel = &panel;
    score_bar.seed_base_panel = true;

    LegacyGameplayFrameResult result;
    assert(render_legacy_gameplay_frame(
        queue, game, terrain, terrain_runtime, horizontal,
        std::span<const LegacyParticleSystem>{}, score_bar, source,
        presentation_config(), display, true, true, result, {}, &error));

    assert(result.score_bar_seeded);
    assert(!result.score_bar_rasterized[0] && !result.score_bar_rasterized[1]);
    assert(result.world.terrain_viewport_copied);
    assert(result.presentation.enabled);
    assert(result.presentation.raw_mode == 1);
    assert(result.presentation.legacy_commit ==
           LegacyPresentationCommit::ImmediateQuickDrawWindowCopyNoSwap);

    // Source canvas proves the recovered producer ordering: score-bar pixels
    // survive at x=416..575 while the completed world replaces x=0..415.
    assert(at(source, 0, 0) == 0x2222);
    assert(at(source, 415, 479) == 0x2222);
    assert(at(source, 416, 0) == 0x3333);
    assert(at(source, 575, 479) == 0x3333);

    // Mode-1 presenter inserts the canonical 32-pixel side borders.
    assert(at(display, 31, 0) == 0x7777);
    assert(at(display, 32, 0) == 0x2222);
    assert(at(display, 447, 479) == 0x2222);
    assert(at(display, 448, 0) == 0x3333);
    assert(at(display, 607, 479) == 0x3333);
    assert(at(display, 608, 0) == 0x7777);

    // Presentation gate suppresses only the final display commit. The source
    // frame still receives score-bar + world production.
    LegacyRasterSurface source2(576, 480, 0x1111);
    LegacyRasterSurface display2(640, 480, 0x7777);
    LegacyRasterSurface terrain2(480, 480, 0x4210);
    LegacyTerrainSurfaceRuntime terrain_runtime2;
    assert(initialize_legacy_terrain_surface_runtime(
        terrain_runtime2, terrain2, terrain_cfg, &error));
    LegacyRasterSurface game2(416, 480, 0);
    LegacyRenderQueue queue2;
    LegacyGameplayFrameResult disabled;
    assert(render_legacy_gameplay_frame(
        queue2, game2, terrain2, terrain_runtime2, horizontal,
        std::span<const LegacyParticleSystem>{}, score_bar, source2,
        presentation_config(), display2, true, false, disabled, {}, &error));
    assert(!disabled.presentation.enabled);
    assert(at(source2, 0, 0) == 0x4210);
    assert(at(source2, 416, 0) == 0x3333);
    assert(at(display2, 32, 0) == 0x7777);

    std::cout << "gameplay_frame_runtime_test: PASS\n";
    return 0;
}
