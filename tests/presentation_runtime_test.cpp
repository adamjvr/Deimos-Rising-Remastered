#include "deimos/presentation_runtime.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>

using namespace deimos;

namespace {
NamedTable<float> canonical_like_table() {
    NamedTable<float> t(61);
    for (std::size_t i = 0; i < t.size(); ++i) t[i] = {"unused" + std::to_string(i), 0.0f};
    t[52] = {"MinScreenWidth", 640.0f};
    t[53] = {"MinScreenHeight", 480.0f};
    t[54] = {"VisibleGameWidth", 416.0f};
    t[55] = {"VisibleGameHeight", 480.0f};
    t[56] = {"ReqDisplayDepth", 16.0f};
    t[57] = {"ScoreBarWidth", 160.0f};
    t[58] = {"ScoreBarHeight", 480.0f};
    t[59] = {"LeftBorderWidth", 32.0f};
    t[60] = {"RightBorderWidth", 32.0f};
    return t;
}

std::uint16_t at(const LegacyRasterSurface& s, int x, int y) {
    return s.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(s.width) +
                    static_cast<std::size_t>(x)];
}
} // namespace

int main() {
    std::string error;
    const auto compiled = compile_legacy_presentation_config(canonical_like_table(), &error);
    assert(compiled);
    const auto cfg = *compiled;
    assert(cfg.min_screen_width == 640);
    assert(cfg.min_screen_height == 480);
    assert(cfg.left_border_width + cfg.visible_game_width + cfg.score_bar_width +
               cfg.right_border_width == cfg.min_screen_width);

    // 0xBC60 mode 0: one complete 640x480 CopyBits into the centered frame.
    LegacyPresentationPlan full;
    assert(plan_legacy_post_world_presentation(cfg, 800, 600, true, 0, full, &error));
    assert(full.enabled);
    assert((full.centered_minimum_frame == LegacyRasterRect{60, 80, 540, 720}));
    assert(full.clear_rects.empty());
    assert(full.copies.size() == 1);
    assert((full.copies[0].source == LegacyRasterRect{0, 0, 480, 640}));
    assert((full.copies[0].destination == LegacyRasterRect{60, 80, 540, 720}));

    // 0xBEB0 mode 1 at minimum resolution: source game and score bar are
    // packed contiguously but land after the 32-pixel left border.
    LegacyPresentationPlan gameplay640;
    assert(plan_legacy_post_world_presentation(cfg, 640, 480, true, 1, gameplay640, &error));
    assert(gameplay640.enabled);
    assert(gameplay640.clear_rects.empty()); // PPC +0x64 border-paint gate is false at exact minimum width.
    assert(gameplay640.copies.size() == 2);
    assert((gameplay640.copies[0].source == LegacyRasterRect{0, 0, 480, 416}));
    assert((gameplay640.copies[0].destination == LegacyRasterRect{0, 32, 480, 448}));
    assert((gameplay640.copies[1].source == LegacyRasterRect{0, 416, 480, 576}));
    assert((gameplay640.copies[1].destination == LegacyRasterRect{0, 448, 480, 608}));

    // A larger display is centered exactly as the setup code computes it;
    // 0xBEB0 explicitly paints the left/right 32-pixel strips.
    LegacyPresentationPlan gameplay800;
    assert(plan_legacy_post_world_presentation(cfg, 800, 600, true, 1, gameplay800, &error));
    assert(gameplay800.clear_rects.size() == 2);
    assert((gameplay800.clear_rects[0] == LegacyRasterRect{60, 80, 540, 112}));
    assert((gameplay800.clear_rects[1] == LegacyRasterRect{60, 688, 540, 720}));
    assert((gameplay800.copies[0].destination == LegacyRasterRect{60, 112, 540, 528}));
    assert((gameplay800.copies[1].destination == LegacyRasterRect{60, 528, 540, 688}));

    // Third 0x30BC0 argument is a strict post-world presentation gate.
    LegacyPresentationPlan disabled;
    assert(plan_legacy_post_world_presentation(cfg, 640, 480, false, 1, disabled, &error));
    assert(!disabled.enabled && disabled.copies.empty());

    // Mode values other than 0/1 fall through 0x30DA0..0x30DCC without a presenter call.
    LegacyPresentationPlan unsupported;
    assert(plan_legacy_post_world_presentation(cfg, 640, 480, true, 2, unsupported, &error));
    assert(!unsupported.enabled && unsupported.copies.empty());

    // Portable execution freezes the recovered geometry independent of QuickDraw.
    LegacyRasterSurface source(640, 480, 0x1111);
    for (int y = 0; y < 480; ++y) {
        for (int x = 0; x < 416; ++x)
            source.pixels[static_cast<std::size_t>(y) * 640 + x] = 0x2222;
        for (int x = 416; x < 576; ++x)
            source.pixels[static_cast<std::size_t>(y) * 640 + x] = 0x3333;
    }
    LegacyRasterSurface display(800, 600, 0x7777);
    assert(execute_legacy_presentation_plan(gameplay800, source, display, &error));
    assert(at(display, 81, 61) == 0x0000);  // left border PaintRect
    assert(at(display, 112, 60) == 0x2222); // first game pixel
    assert(at(display, 527, 539) == 0x2222); // last game pixel
    assert(at(display, 528, 60) == 0x3333); // first score-bar pixel
    assert(at(display, 687, 539) == 0x3333); // last score-bar pixel
    assert(at(display, 688, 60) == 0x0000); // right border PaintRect
    assert(at(display, 10, 10) == 0x7777);  // outside centered minimum frame untouched

    auto bad = canonical_like_table();
    bad[59].first = "NotLeftBorderWidth";
    assert(!compile_legacy_presentation_config(bad, &error));

    std::cout << "presentation_runtime_test: PASS\n";
    return 0;
}
