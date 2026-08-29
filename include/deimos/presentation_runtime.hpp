#pragma once

#include "deimos/data_tables.hpp"
#include "deimos/render_backend.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace deimos {

// Game[gafl] 52..60, consumed by the original display/presentation manager
// around PPC 0xAE20..0xB51C and the presenters at 0xBC60/0xBEB0.
struct LegacyPresentationConfig {
    int min_screen_width = 640;
    int min_screen_height = 480;
    int visible_game_width = 416;
    int visible_game_height = 480;
    int display_depth = 16;
    int score_bar_width = 160;
    int score_bar_height = 480;
    int left_border_width = 32;
    int right_border_width = 32;

    constexpr bool operator==(const LegacyPresentationConfig&) const = default;
};

[[nodiscard]] std::optional<LegacyPresentationConfig> compile_legacy_presentation_config(
    const NamedTable<float>& game_floats,
    std::string* error = nullptr);

enum class LegacyPresentationMode : std::uint8_t {
    FullFrame = 0, // PPC 0xBC60: menus/non-gameplay paths
    Gameplay = 1  // PPC 0xBEB0: normal gameplay path
};

struct LegacyPresentationCopy {
    LegacyRasterRect source{};
    LegacyRasterRect destination{};
    constexpr bool operator==(const LegacyPresentationCopy&) const = default;
};

// Historical commit mechanism of the original Mac renderer. This describes
// what the recovered 1.0.6 executable did after composing the source canvas;
// it does not constrain a modern backend's own swapchain/present API.
enum class LegacyPresentationCommit : std::uint8_t {
    None = 0,
    ImmediateQuickDrawWindowCopyNoSwap = 1
};

struct LegacyPresentationPlan {
    bool enabled = false;
    std::uint8_t raw_mode = 0;
    LegacyPresentationCommit legacy_commit = LegacyPresentationCommit::None;
    LegacyRasterRect centered_minimum_frame{};
    std::vector<LegacyRasterRect> clear_rects;
    std::vector<LegacyPresentationCopy> copies;
};

// Models the post-world stage at 0x30D8C..0x30DCC. The third 0x30BC0
// argument gates this stage. Mode 0 calls 0xBC60; mode 1 calls 0xBEB0;
// all other mode values perform no presentation call.
[[nodiscard]] bool plan_legacy_post_world_presentation(
    const LegacyPresentationConfig& config,
    int display_width,
    int display_height,
    bool presentation_enabled,
    std::uint8_t frame_mode,
    LegacyPresentationPlan& plan,
    std::string* error = nullptr);

// Portable pixel execution of the recovered CopyBits/PaintRect plan. This is
// deliberately a clean-core reference path. The legacy executable committed
// synchronously into its QuickDraw window port and imported no DrawSprocket
// back-buffer/swap API; modern backends may still use their own native
// swapchain after executing/mapping this plan.
[[nodiscard]] bool execute_legacy_presentation_plan(
    const LegacyPresentationPlan& plan,
    const LegacyRasterSurface& source_canvas,
    LegacyRasterSurface& destination,
    std::string* error = nullptr);

} // namespace deimos
