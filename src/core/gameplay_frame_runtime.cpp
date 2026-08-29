#include "deimos/gameplay_frame_runtime.hpp"

#include <algorithm>
#include <cstddef>

namespace deimos {
namespace {

bool fail(std::string* error, const char* message) {
    if (error) *error = message;
    return false;
}

bool seed_score_bar_panel(
    const LegacyRasterSurface& base,
    LegacyRasterSurface& source,
    std::string* error) {
    if (!base.valid() || base.width != 160 || base.height != 480)
        return fail(error, "score-bar base panel is not the canonical 160x480 surface");
    if (!source.valid() || source.width < 576 || source.height < 480)
        return fail(error, "presentation source is smaller than the recovered 576x480 gameplay source");

    for (int y = 0; y < 480; ++y) {
        const auto src = base.pixels.begin() + static_cast<std::ptrdiff_t>(y) * 160;
        const auto dst = source.pixels.begin() + static_cast<std::ptrdiff_t>(y) * source.width + 416;
        std::copy_n(src, 160, dst);
    }
    return true;
}

bool copy_game_to_source(
    const LegacyRasterSurface& game,
    LegacyRasterSurface& source,
    const LegacyPresentationConfig& config,
    std::string* error) {
    if (!game.valid() || game.width != config.visible_game_width ||
        game.height != config.visible_game_height)
        return fail(error, "game surface does not match the recovered visible-game dimensions");
    if (!source.valid() || source.width < config.visible_game_width + config.score_bar_width ||
        source.height < config.min_screen_height)
        return fail(error, "presentation source does not contain the recovered game+score-bar extent");

    for (int y = 0; y < config.visible_game_height; ++y) {
        const auto src = game.pixels.begin() + static_cast<std::ptrdiff_t>(y) * game.width;
        const auto dst = source.pixels.begin() + static_cast<std::ptrdiff_t>(y) * source.width;
        std::copy_n(src, config.visible_game_width, dst);
    }
    return true;
}

} // namespace

bool render_legacy_gameplay_frame(
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
    LegacyRasterConfig raster_config,
    std::string* error) {
    result = {};

    if (presentation_config.visible_game_width != terrain_runtime.config.visible_width ||
        presentation_config.visible_game_height != terrain_runtime.config.visible_height)
        return fail(error, "presentation and terrain visible-game dimensions disagree");
    if (presentation_source.width < presentation_config.visible_game_width + presentation_config.score_bar_width ||
        presentation_source.height < presentation_config.min_screen_height)
        return fail(error, "presentation source is smaller than the recovered gameplay source extent");

    // Main gameplay loop order: score-bar draw precedes 0x30570/0x30BC0.
    if (score_bar.seed_base_panel) {
        if (!score_bar.assets.base_panel)
            return fail(error, "score-bar panel seeding requested without base panel asset");
        if (!seed_score_bar_panel(*score_bar.assets.base_panel, presentation_source, error)) return false;
        result.score_bar_seeded = true;
    }

    const bool any_score_bar_player = score_bar.players[0] || score_bar.players[1];
    if (any_score_bar_player) {
        if (!score_bar.config || !score_bar.styles)
            return fail(error, "score-bar player rendering requires config and text styles");
        for (int i = 0; i < 2; ++i) {
            if (!score_bar.players[i]) continue;
            if (!rasterize_legacy_score_bar_player(
                    i, *score_bar.players[i], *score_bar.config, *score_bar.styles,
                    score_bar.assets, presentation_source, error))
                return false;
            result.score_bar_rasterized[i] = true;
        }
    }

    if (!render_legacy_world_frame(
            queue, game_surface, persistent_terrain, terrain_runtime, horizontal_view,
            particle_systems, world_draw_enabled, result.world, raster_config, error))
        return false;

    if (!copy_game_to_source(game_surface, presentation_source, presentation_config, error)) return false;

    if (!plan_legacy_post_world_presentation(
            presentation_config, display_surface.width, display_surface.height,
            presentation_enabled, static_cast<std::uint8_t>(LegacyPresentationMode::Gameplay),
            result.presentation, error))
        return false;

    return execute_legacy_presentation_plan(result.presentation, presentation_source, display_surface, error);
}

} // namespace deimos
