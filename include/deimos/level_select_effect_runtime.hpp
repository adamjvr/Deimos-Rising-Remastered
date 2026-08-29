#pragma once

#include "deimos/data_tables.hpp"
#include "deimos/render_backend.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace deimos {

// Front-end-only pulse owned by the level-selection path around
// 0x2F7A0/0x2FC90/0x2FCC0/0x2FE40. It is deliberately kept outside the
// gameplay-frame runtime: the PPC callers use it for level-selection
// acceptance/failure feedback, not for the in-game HUD.
enum class LegacyLevelSelectEffectKind : std::uint8_t {
    inactive = 0,
    acceptance = 1,
    failure = 2,
};

struct LegacyLevelSelectEffectConfig {
    float acceptance_scaling_rate = 0.18f;
    float acceptance_max_scale = 2.0f;
    float failure_scaling_rate = 0.25f;
    float failure_max_scale = 2.0f;
};

// Game[gafl] 44..47, label-verified. The values are the exact float-domain
// inputs selected by 0x2FCC0 for active state 1 versus state 2.
[[nodiscard]] std::optional<LegacyLevelSelectEffectConfig>
compile_legacy_level_select_effect_config(
    const NamedTable<float>& game_floats,
    std::string* error = nullptr);

struct LegacyLevelSelectEffectStyle {
    std::uint16_t color_rgb555 = 0x7fff;
    int blend_amount_0_to_32 = 32;
};

// 0x2FE40 reads only the compiled text-format color (+0x122) and blend
// (+0x114) from Formats[gate] runtime ordinals 27/28: lsca/lscf.
[[nodiscard]] LegacyLevelSelectEffectStyle
compile_legacy_level_select_effect_style(const TextFormatDefinition& format);

struct LegacyLevelSelectEffectState {
    LegacyLevelSelectEffectKind kind = LegacyLevelSelectEffectKind::inactive;
    std::uint16_t color_rgb555 = 0x7fff;
    int blend_amount_0_to_32 = 32;
    float scale = 0.0f;
    bool expanding = false;
};

// 0x2FC90 default/reset state.
void reset_legacy_level_select_effect(LegacyLevelSelectEffectState& state);

// 0x2FE40 mode contract: 0 resets, 1 starts acceptance, 2 starts failure;
// other values leave the current state untouched.
void trigger_legacy_level_select_effect(
    LegacyLevelSelectEffectState& state,
    int mode,
    const LegacyLevelSelectEffectStyle& acceptance_style,
    const LegacyLevelSelectEffectStyle& failure_style);

// 0x2FCC0. Blend moves one step toward 32 every active tick. Scale performs a
// 0 -> max -> 0 pulse at the mode-specific rate. Once both scale==0 and
// blend==32, the state returns to the exact 0x2FC90 defaults.
void advance_legacy_level_select_effect(
    LegacyLevelSelectEffectState& state,
    const LegacyLevelSelectEffectConfig& config);

// Geometry portion of the 0x2F9E4..0x2FB80 overlay path, after its surrounding
// coordinate-space conversion has produced the target rectangle. The original
// effect scales the rectangle around its integer center with PPC fctiwz-style
// truncation, then submits a COST solid-color request.
[[nodiscard]] LegacyRasterRect scale_legacy_level_select_effect_rect(
    LegacyRasterRect base,
    float scale);

// Build the immediate COST request used by 0x2FB88..0x2FC14. Inactive, zero-
// scale, or fully transparent (32) states return no request.
[[nodiscard]] std::optional<LegacyRasterRequest>
build_legacy_level_select_effect_request(
    const LegacyLevelSelectEffectState& state,
    LegacyRasterRect base,
    LegacyRasterRect clip);

} // namespace deimos
