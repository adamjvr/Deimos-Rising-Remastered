#pragma once

#include "deimos/gameplay_frame_runtime.hpp"
#include "deimos/render_orchestration.hpp"
#include "deimos/render_runtime.hpp"
#include "deimos/score_bar_runtime.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace deimos {

// External-original-data integration fixture used to prove that the recovered
// clean renderer can produce a real Deimos frame without embedding any
// copyrighted game assets in this repository. The caller supplies a directory
// containing the user's original Game.pak and Interface.pak.
struct OriginalGameFramePreviewInfo {
    FourCC level_id{};
    std::string level_name;
    FourCC background_id{};
    std::string player_name;
    FourCC player_face{};
    int player_frame = 0;
    std::size_t loaded_sprite_groups = 0;
    float fps_max_rate = 30.0f;
};

// Canonical user-owned 1.0.6 Level-1 / Player-1 first-frame oracle. This
// freezes the complete 640x480 xRGB1555 result proven on macOS Metal without
// embedding any original data in the source tree.
inline constexpr std::uint64_t kCanonicalOriginalGameInitialFrameFnv64 =
    0x9e8a7ec73b79b254ull;
inline constexpr std::uint64_t kCanonicalOriginalGameTick1FrameFnv64 =
    0x44dede08075273f2ull;
inline constexpr std::uint64_t kCanonicalOriginalGameTick30FrameFnv64 =
    0x51d4a7eec9b0beefull;

struct OriginalGameFrameTickResult {
    std::uint64_t tick_index = 0;
    int terrain_source_top = 0;
    int terrain_applied_vertical_delta = 0;
    bool terrain_reached_end = false;
};

class OriginalGameFramePreview {
public:
    OriginalGameFramePreview() = default;
    OriginalGameFramePreview(OriginalGameFramePreview&&) noexcept = default;
    OriginalGameFramePreview& operator=(OriginalGameFramePreview&&) noexcept = default;
    OriginalGameFramePreview(const OriginalGameFramePreview&) = delete;
    OriginalGameFramePreview& operator=(const OriginalGameFramePreview&) = delete;

    // Load only the resources required for a deterministic Level-1 preview:
    // level background, score-bar panel/font/styles, player score-bar sprites,
    // three canonical weapon previews, and the Player-1 life/main sprite.
    [[nodiscard]] static std::optional<OriginalGameFramePreview> load(
        const std::filesystem::path& pak_directory,
        FourCC level_id = FourCC{{'l','e','0','1'}},
        int player_index = 0,
        std::string* error = nullptr);

    // Execute the already-recovered gameplay-frame boundary and return the
    // canonical 640x480 xRGB1555 display frame. This is deliberately a static
    // integration preview, not yet the live simulation/game loop.
    [[nodiscard]] bool render(
        LegacyRasterSurface& canonical_display,
        LegacyGameplayFrameResult* frame_result = nullptr,
        std::string* error = nullptr);

    // Advance one deterministic recovered gameplay tick. The current live
    // preview intentionally advances only subsystems whose exact outer-loop
    // semantics are already closed: terrain scroll and score-bar convergence.
    // Player input, entities, collisions and spawns remain separate upcoming
    // integrations rather than being approximated here.
    [[nodiscard]] OriginalGameFrameTickResult tick();

    [[nodiscard]] std::uint64_t ticks_elapsed() const noexcept { return ticks_elapsed_; }
    [[nodiscard]] const OriginalGameFramePreviewInfo& info() const noexcept { return info_; }
    [[nodiscard]] const LegacyPresentationConfig& presentation_config() const noexcept {
        return presentation_config_;
    }

private:
    OriginalGameFramePreviewInfo info_{};
    LegacyPresentationConfig presentation_config_{};
    LegacyTerrainSurfaceConfig terrain_config_{};
    LegacyTerrainSurfaceRuntime terrain_runtime_{};
    LegacyHorizontalViewRuntime horizontal_view_{};
    LegacyShadowRuntimeConfig shadow_config_{};
    LegacyScoreBarConfig score_bar_config_{};
    LegacyScoreBarTextStyles score_bar_styles_{};

    LegacyRasterSurface persistent_terrain_{};
    LegacyRasterSurface score_bar_panel_{};
    LegacySpriteGroupMetadata small_text_font_{};
    LegacySpriteCache sprite_cache_{};
    LegacyScoreBarPlayerState score_bar_player_{};
    LegacyScoreBarWeaponInput score_bar_weapon_input_{};
    PlayerRuntimeSlot player_runtime_{};
    CompiledPlayerRuntimeDefinition player_definition_{};

    LegacySpriteVisualRuntime player_visual_{};
    float player_x_ = 0.0f;
    float player_y_ = 0.0f;

    // Persistent visible-frame objects for the live host path. The first
    // render seeds the static score-bar panel; later frames preserve the
    // presentation source and redraw only dirty HUD classes.
    LegacyRasterSurface game_surface_{};
    LegacyRasterSurface presentation_source_{};
    LegacyRasterSurface display_surface_{};
    LegacyRenderQueue render_queue_{};
    std::uint64_t ticks_elapsed_ = 0;
    std::uint32_t render_sequence_ = 1;
    bool first_render_ = true;
};

// Utility shared by the smoke app/tool. A valid directory must contain both
// Game.pak and Interface.pak; no files are copied into the repository/build.
[[nodiscard]] bool original_game_pak_directory_valid(
    const std::filesystem::path& pak_directory) noexcept;

} // namespace deimos
