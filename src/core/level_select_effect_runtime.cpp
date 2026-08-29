#include "deimos/level_select_effect_runtime.hpp"

#include "deimos/score_bar_runtime.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>

namespace deimos {
namespace {

constexpr FourCC fourcc(char a, char b, char c, char d) {
    return FourCC{{a, b, c, d}};
}

void fail(std::string* error, std::string message) {
    if (error) *error = std::move(message);
}

float rate_for(
    LegacyLevelSelectEffectKind kind,
    const LegacyLevelSelectEffectConfig& config) {
    return kind == LegacyLevelSelectEffectKind::failure
        ? config.failure_scaling_rate
        : config.acceptance_scaling_rate;
}

float max_scale_for(
    LegacyLevelSelectEffectKind kind,
    const LegacyLevelSelectEffectConfig& config) {
    return kind == LegacyLevelSelectEffectKind::failure
        ? config.failure_max_scale
        : config.acceptance_max_scale;
}

int trunc_i(float v) {
    return static_cast<int>(std::trunc(v));
}

} // namespace

std::optional<LegacyLevelSelectEffectConfig>
compile_legacy_level_select_effect_config(
    const NamedTable<float>& game_floats,
    std::string* error) {
    constexpr std::size_t first = 44;
    constexpr std::array<std::string_view, 4> labels = {{
        "LevSel_Acceptance_ScalingRate",
        "LevSel_Acceptance_MaxScale",
        "LevSel_Failure_ScalingRate",
        "LevSel_Failure_MaxScale",
    }};
    if (game_floats.size() < first + labels.size()) {
        fail(error, "Game[gafl] is shorter than the 1.0.6 level-select effect contract");
        return std::nullopt;
    }
    for (std::size_t i = 0; i < labels.size(); ++i) {
        if (game_floats[first + i].first != labels[i]) {
            fail(error, "unexpected Game[gafl] level-select label at index " +
                            std::to_string(first + i));
            return std::nullopt;
        }
    }

    LegacyLevelSelectEffectConfig out;
    out.acceptance_scaling_rate = game_floats[first + 0].second;
    out.acceptance_max_scale = game_floats[first + 1].second;
    out.failure_scaling_rate = game_floats[first + 2].second;
    out.failure_max_scale = game_floats[first + 3].second;
    return out;
}

LegacyLevelSelectEffectStyle
compile_legacy_level_select_effect_style(const TextFormatDefinition& format) {
    LegacyLevelSelectEffectStyle out;
    out.color_rgb555 = legacy_rgb24_to_rgb555(format.colorise_color);
    out.blend_amount_0_to_32 = format.blend_amount_0_to_32;
    return out;
}

void reset_legacy_level_select_effect(LegacyLevelSelectEffectState& state) {
    state.kind = LegacyLevelSelectEffectKind::inactive;
    state.color_rgb555 = 0x7fff;
    state.blend_amount_0_to_32 = 32;
    state.scale = 0.0f;
    state.expanding = false;
}

void trigger_legacy_level_select_effect(
    LegacyLevelSelectEffectState& state,
    int mode,
    const LegacyLevelSelectEffectStyle& acceptance_style,
    const LegacyLevelSelectEffectStyle& failure_style) {
    if (mode == 0) {
        reset_legacy_level_select_effect(state);
        return;
    }

    const LegacyLevelSelectEffectStyle* style = nullptr;
    if (mode == 1) {
        state.kind = LegacyLevelSelectEffectKind::acceptance;
        style = &acceptance_style;
    } else if (mode == 2) {
        state.kind = LegacyLevelSelectEffectKind::failure;
        style = &failure_style;
    } else {
        return;
    }

    state.scale = 0.0f;
    state.expanding = true;
    state.color_rgb555 = style->color_rgb555;
    state.blend_amount_0_to_32 = style->blend_amount_0_to_32;
}

void advance_legacy_level_select_effect(
    LegacyLevelSelectEffectState& state,
    const LegacyLevelSelectEffectConfig& config) {
    if (state.kind == LegacyLevelSelectEffectKind::inactive) return;

    if (state.blend_amount_0_to_32 != 32) {
        ++state.blend_amount_0_to_32;
        if (state.blend_amount_0_to_32 > 32) state.blend_amount_0_to_32 = 32;
    }

    const float rate = rate_for(state.kind, config);
    const float maximum = max_scale_for(state.kind, config);
    if (state.expanding) {
        state.scale += rate;
        if (state.scale >= maximum) {
            state.scale = maximum;
            state.expanding = false;
        }
    } else {
        state.scale -= rate;
        if (state.scale <= 0.0f) state.scale = 0.0f;
    }

    if (state.scale == 0.0f && state.blend_amount_0_to_32 == 32) {
        reset_legacy_level_select_effect(state);
    }
}

LegacyRasterRect scale_legacy_level_select_effect_rect(
    LegacyRasterRect base,
    float scale) {
    if (base.empty() || scale <= 0.0f) return {};

    const int width = base.right - base.left;
    const int height = base.bottom - base.top;
    const int center_x = base.left + width / 2;
    const int center_y = base.top + height / 2;

    const float scaled_width_f = static_cast<float>(width) * scale;
    const float scaled_height_f = static_cast<float>(height) * scale;
    const int scaled_width = trunc_i(scaled_width_f);
    const int scaled_height = trunc_i(scaled_height_f);

    const int left = trunc_i(static_cast<float>(center_x) - scaled_width_f * 0.5f);
    const int top = trunc_i(static_cast<float>(center_y) - scaled_height_f * 0.5f);
    return {top, left, top + scaled_height, left + scaled_width};
}

std::optional<LegacyRasterRequest>
build_legacy_level_select_effect_request(
    const LegacyLevelSelectEffectState& state,
    LegacyRasterRect base,
    LegacyRasterRect clip) {
    if (state.kind == LegacyLevelSelectEffectKind::inactive ||
        state.scale == 0.0f || state.blend_amount_0_to_32 >= 32) {
        return std::nullopt;
    }

    const auto rect = scale_legacy_level_select_effect_rect(base, state.scale);
    if (rect.empty()) return std::nullopt;

    LegacyRasterRequest request;
    request.sprite_face = fourcc('C', 'O', 'S', 'T');
    request.effect_amount_0_to_32 = state.blend_amount_0_to_32;
    request.clip = clip;
    request.immediate = true;
    request.special_rect = rect;
    request.special_color = state.color_rgb555;
    return request;
}

} // namespace deimos
