#include "deimos/level_select_effect_runtime.hpp"

#include <cassert>
#include <cmath>
#include <string>

namespace {

deimos::NamedTable<float> make_game_floats() {
    deimos::NamedTable<float> table;
    for (int i = 0; i < 44; ++i) table.push_back({"unused" + std::to_string(i), 0.0f});
    table.push_back({"LevSel_Acceptance_ScalingRate", 0.18f});
    table.push_back({"LevSel_Acceptance_MaxScale", 2.0f});
    table.push_back({"LevSel_Failure_ScalingRate", 0.25f});
    table.push_back({"LevSel_Failure_MaxScale", 2.0f});
    return table;
}

} // namespace

int main() {
    using namespace deimos;

    std::string error;
    const auto config = compile_legacy_level_select_effect_config(make_game_floats(), &error);
    assert(config);
    assert(config->acceptance_scaling_rate == 0.18f);
    assert(config->acceptance_max_scale == 2.0f);
    assert(config->failure_scaling_rate == 0.25f);
    assert(config->failure_max_scale == 2.0f);

    auto shifted = make_game_floats();
    shifted[44].first = "wrong";
    assert(!compile_legacy_level_select_effect_config(shifted, &error));

    TextFormatDefinition green;
    green.blend_amount_0_to_32 = 16;
    green.colorise_color = {0x00, 0xff, 0x00};
    TextFormatDefinition red;
    red.blend_amount_0_to_32 = 16;
    red.colorise_color = {0xff, 0x00, 0x00};
    const auto accept = compile_legacy_level_select_effect_style(green);
    const auto fail = compile_legacy_level_select_effect_style(red);
    assert(accept.color_rgb555 == 0x03e0);
    assert(fail.color_rgb555 == 0x7c00);
    assert(accept.blend_amount_0_to_32 == 16);

    LegacyLevelSelectEffectState state;
    reset_legacy_level_select_effect(state);
    assert(state.kind == LegacyLevelSelectEffectKind::inactive);
    assert(state.color_rgb555 == 0x7fff);
    assert(state.blend_amount_0_to_32 == 32);
    assert(state.scale == 0.0f);
    assert(!state.expanding);

    trigger_legacy_level_select_effect(state, 1, accept, fail);
    assert(state.kind == LegacyLevelSelectEffectKind::acceptance);
    assert(state.color_rgb555 == 0x03e0);
    assert(state.blend_amount_0_to_32 == 16);
    assert(state.scale == 0.0f);
    assert(state.expanding);
    advance_legacy_level_select_effect(state, *config);
    assert(state.blend_amount_0_to_32 == 17);
    assert(std::fabs(state.scale - 0.18f) < 0.00001f);

    // The 0.18/2.0 acceptance pulse needs 12 ticks to reach max and another
    // 12 to return to zero. Blend reaches 32 earlier; teardown waits for both.
    int acceptance_ticks = 1;
    while (state.kind != LegacyLevelSelectEffectKind::inactive && acceptance_ticks < 100) {
        advance_legacy_level_select_effect(state, *config);
        ++acceptance_ticks;
    }
    assert(acceptance_ticks == 24);
    assert(state.color_rgb555 == 0x7fff);
    assert(state.blend_amount_0_to_32 == 32);

    trigger_legacy_level_select_effect(state, 2, accept, fail);
    assert(state.kind == LegacyLevelSelectEffectKind::failure);
    assert(state.color_rgb555 == 0x7c00);
    int failure_ticks = 0;
    while (state.kind != LegacyLevelSelectEffectKind::inactive && failure_ticks < 100) {
        advance_legacy_level_select_effect(state, *config);
        ++failure_ticks;
    }
    assert(failure_ticks == 16);

    // Unsupported trigger values are inert, not aliases for failure/reset.
    trigger_legacy_level_select_effect(state, 1, accept, fail);
    const auto before = state;
    trigger_legacy_level_select_effect(state, 3, accept, fail);
    assert(state.kind == before.kind);
    assert(state.color_rgb555 == before.color_rgb555);
    assert(state.blend_amount_0_to_32 == before.blend_amount_0_to_32);
    assert(state.scale == before.scale);

    // Geometry is center-preserving and PPC/fctiwz-truncated.
    const LegacyRasterRect base{10, 20, 30, 60};; // 40x20, center 40,20
    assert(scale_legacy_level_select_effect_rect(base, 1.0f) == base);
    assert((scale_legacy_level_select_effect_rect(base, 2.0f) == LegacyRasterRect{0,0,40,80}));
    assert(scale_legacy_level_select_effect_rect(base, 0.0f).empty());

    state.kind = LegacyLevelSelectEffectKind::acceptance;
    state.color_rgb555 = 0x03e0;
    state.blend_amount_0_to_32 = 16;
    state.scale = 1.0f;
    state.expanding = false;
    const auto request = build_legacy_level_select_effect_request(state, base, {0,0,100,100});
    assert(request);
    const FourCC cost{{'C','O','S','T'}};
    assert(request->sprite_face == cost);
    assert(request->special_rect == base);
    assert(request->special_color == 0x03e0);
    assert(request->effect_amount_0_to_32 == 16);
    assert(request->immediate);

    LegacyRasterSurface surface(100, 100, 0x001f);
    assert(rasterize_legacy_request(*request, surface) == LegacyRasterResult::drawn);
    // 50/50 green over blue at blend=16.
    assert(surface.pixels[10 * 100 + 20] == legacy_blend_rgb555(0x001f, 0x03e0, 16));

    state.blend_amount_0_to_32 = 32;
    assert(!build_legacy_level_select_effect_request(state, base, {0,0,100,100}));

    return 0;
}
