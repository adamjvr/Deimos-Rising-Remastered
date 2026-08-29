#include "deimos/presentation_runtime.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>

namespace deimos {
namespace {

bool valid_config(const LegacyPresentationConfig& c, std::string* error) {
    if (c.min_screen_width <= 0 || c.min_screen_height <= 0 ||
        c.visible_game_width <= 0 || c.visible_game_height <= 0 ||
        c.score_bar_width <= 0 || c.score_bar_height <= 0 ||
        c.left_border_width < 0 || c.right_border_width < 0) {
        if (error) *error = "presentation dimensions must be positive (borders may be zero)";
        return false;
    }
    if (c.display_depth != 16) {
        if (error) *error = "legacy presentation contract requires 16-bit display depth";
        return false;
    }
    if (c.visible_game_height != c.min_screen_height ||
        c.score_bar_height != c.min_screen_height) {
        if (error) *error = "1.0.6 presentation requires game and score-bar heights to match MinScreenHeight";
        return false;
    }
    if (c.left_border_width + c.visible_game_width + c.score_bar_width + c.right_border_width !=
        c.min_screen_width) {
        if (error) *error = "presentation widths do not sum to MinScreenWidth";
        return false;
    }
    return true;
}

bool copy_rect(
    const LegacyRasterSurface& src,
    LegacyRasterSurface& dst,
    const LegacyRasterRect& s,
    const LegacyRasterRect& d,
    std::string* error) {
    if (s.empty() || d.empty()) return true;
    const int sw = s.right - s.left;
    const int sh = s.bottom - s.top;
    const int dw = d.right - d.left;
    const int dh = d.bottom - d.top;
    if (sw != dw || sh != dh) {
        if (error) *error = "clean presentation executor requires 1:1 CopyBits rectangles";
        return false;
    }
    if (s.top < 0 || s.left < 0 || s.bottom > src.height || s.right > src.width ||
        d.top < 0 || d.left < 0 || d.bottom > dst.height || d.right > dst.width) {
        if (error) *error = "presentation copy rectangle exceeds a surface";
        return false;
    }
    for (int y = 0; y < sh; ++y) {
        const auto src_begin = src.pixels.begin() +
            static_cast<std::ptrdiff_t>(s.top + y) * src.width + s.left;
        const auto dst_begin = dst.pixels.begin() +
            static_cast<std::ptrdiff_t>(d.top + y) * dst.width + d.left;
        std::copy_n(src_begin, sw, dst_begin);
    }
    return true;
}

} // namespace

std::optional<LegacyPresentationConfig> compile_legacy_presentation_config(
    const NamedTable<float>& game_floats,
    std::string* error) {
    constexpr std::size_t kFirst = 52;
    constexpr std::array<std::string_view, 9> labels = {
        "MinScreenWidth", "MinScreenHeight", "VisibleGameWidth", "VisibleGameHeight",
        "ReqDisplayDepth", "ScoreBarWidth", "ScoreBarHeight", "LeftBorderWidth",
        "RightBorderWidth"
    };
    if (game_floats.size() < kFirst + labels.size()) {
        if (error) *error = "Game[gafl] is shorter than the 1.0.6 presentation positional contract";
        return std::nullopt;
    }
    for (std::size_t i = 0; i < labels.size(); ++i) {
        if (game_floats[kFirst + i].first != labels[i]) {
            if (error) {
                *error = "unexpected Game[gafl] presentation label at index " +
                    std::to_string(kFirst + i);
            }
            return std::nullopt;
        }
    }

    const auto trunc_i = [&](std::size_t i) {
        return static_cast<int>(std::trunc(game_floats[i].second));
    };
    LegacyPresentationConfig out;
    out.min_screen_width = trunc_i(52);
    out.min_screen_height = trunc_i(53);
    out.visible_game_width = trunc_i(54);
    out.visible_game_height = trunc_i(55);
    out.display_depth = trunc_i(56);
    out.score_bar_width = trunc_i(57);
    out.score_bar_height = trunc_i(58);
    out.left_border_width = trunc_i(59);
    out.right_border_width = trunc_i(60);
    if (!valid_config(out, error)) return std::nullopt;
    return out;
}

bool plan_legacy_post_world_presentation(
    const LegacyPresentationConfig& config,
    int display_width,
    int display_height,
    bool presentation_enabled,
    std::uint8_t frame_mode,
    LegacyPresentationPlan& plan,
    std::string* error) {
    plan = {};
    plan.raw_mode = frame_mode;
    if (!valid_config(config, error)) return false;
    if (display_width < config.min_screen_width || display_height < config.min_screen_height) {
        if (error) *error = "display is smaller than the recovered minimum 640x480-class frame";
        return false;
    }

    const int frame_left = (display_width - config.min_screen_width) / 2;
    const int frame_top = (display_height - config.min_screen_height) / 2;
    plan.centered_minimum_frame = {
        frame_top,
        frame_left,
        frame_top + config.min_screen_height,
        frame_left + config.min_screen_width
    };

    if (!presentation_enabled) return true;
    if (frame_mode > static_cast<std::uint8_t>(LegacyPresentationMode::Gameplay)) return true;
    plan.enabled = true;

    if (frame_mode == static_cast<std::uint8_t>(LegacyPresentationMode::FullFrame)) {
        plan.copies.push_back({
            {0, 0, config.min_screen_height, config.min_screen_width},
            plan.centered_minimum_frame
        });
        return true;
    }

    // 0xBEB0 normal gameplay: the offscreen source packs game at x=0..416
    // and score bar at x=416..576. The final 640-wide frame inserts the two
    // 32-pixel borders around those regions.
    const int game_left = frame_left + config.left_border_width;
    const int score_left = game_left + config.visible_game_width;
    const int right_border_left = score_left + config.score_bar_width;

    // Original byte +0x64 becomes true when the host frame is wider than the
    // minimum, causing PaintRect on the two border strips before CopyBits.
    if (display_width > config.min_screen_width) {
        plan.clear_rects.push_back({
            frame_top, frame_left,
            frame_top + config.min_screen_height,
            game_left
        });
        plan.clear_rects.push_back({
            frame_top, right_border_left,
            frame_top + config.min_screen_height,
            frame_left + config.min_screen_width
        });
    }

    plan.copies.push_back({
        {0, 0, config.visible_game_height, config.visible_game_width},
        {frame_top, game_left,
         frame_top + config.visible_game_height,
         game_left + config.visible_game_width}
    });
    plan.copies.push_back({
        {0, config.visible_game_width,
         config.score_bar_height, config.visible_game_width + config.score_bar_width},
        {frame_top, score_left,
         frame_top + config.score_bar_height,
         score_left + config.score_bar_width}
    });
    return true;
}

bool execute_legacy_presentation_plan(
    const LegacyPresentationPlan& plan,
    const LegacyRasterSurface& source_canvas,
    LegacyRasterSurface& destination,
    std::string* error) {
    if (!source_canvas.valid() || !destination.valid()) {
        if (error) *error = "presentation surfaces are invalid";
        return false;
    }
    if (!plan.enabled) return true;

    for (const auto& r : plan.clear_rects) {
        if (r.top < 0 || r.left < 0 || r.bottom > destination.height || r.right > destination.width) {
            if (error) *error = "presentation clear rectangle exceeds destination surface";
            return false;
        }
        for (int y = r.top; y < r.bottom; ++y) {
            auto begin = destination.pixels.begin() +
                static_cast<std::ptrdiff_t>(y) * destination.width + r.left;
            std::fill_n(begin, r.right - r.left, std::uint16_t{0});
        }
    }
    for (const auto& op : plan.copies) {
        if (!copy_rect(source_canvas, destination, op.source, op.destination, error)) return false;
    }
    return true;
}

} // namespace deimos
