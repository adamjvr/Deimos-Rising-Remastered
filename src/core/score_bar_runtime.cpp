#include "deimos/score_bar_runtime.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>

namespace deimos {
namespace {

void fail(std::string* error, std::string message) {
    if (error) *error = std::move(message);
}

int trunc_i(float value) {
    return static_cast<int>(value);
}

void set_all_dirty(LegacyScoreBarDirty& dirty, bool value) {
    dirty.score = value;
    dirty.life_symbol = value;
    dirty.life_count = value;
    dirty.weapons = value;
    dirty.shield = value;
    dirty.power = value;
}

float converge_meter(float cached, float target, float increase_rate, float decrease_rate, bool allow_increase) {
    if (cached > target) {
        cached -= decrease_rate;
        if (cached < target) cached = target;
    } else if (cached < target && allow_increase) {
        cached += increase_rate;
        if (cached > target) cached = target;
    }
    return cached;
}

} // namespace

std::optional<LegacyScoreBarConfig> compile_legacy_score_bar_config(
    const NamedTable<float>& game_floats,
    const NamedTable<RectI>& game_rects,
    std::string* error) {
    constexpr std::size_t first_float = 111;
    constexpr std::array<std::string_view, 33> labels = {{
        "ScoreBar_ScoreSpacing",
        "ScoreBar_P1LivesSymbol_XLoc", "ScoreBar_P1LivesSymbol_YLoc",
        "ScoreBar_P2LivesSymbol_XLoc", "ScoreBar_P2LivesSymbol_YLoc",
        "ScoreBar_P1ShieldMeter_XLoc", "ScoreBar_P1ShieldMeter_YLoc",
        "ScoreBar_P2ShieldMeter_XLoc", "ScoreBar_P2ShieldMeter_YLoc",
        "ScoreBar_ShieldMeterIncreaseRate", "ScoreBar_ShieldMeterDecreaseRate",
        "ScoreBar_P1PowerMeter_XLoc", "ScoreBar_P1PowerMeter_YLoc",
        "ScoreBar_P2PowerMeter_XLoc", "ScoreBar_P2PowerMeter_YLoc",
        "ScoreBar_PowerMeterIncreaseRate", "ScoreBar_PowerMeterDecreaseRate",
        "ScoreBar_P1Weapons_1_XLoc", "ScoreBar_P1Weapons_1_YLoc",
        "ScoreBar_P1Weapons_2_XLoc", "ScoreBar_P1Weapons_2_YLoc",
        "ScoreBar_P1Weapons_3_XLoc", "ScoreBar_P1Weapons_3_YLoc",
        "ScoreBar_P2Weapons_1_XLoc", "ScoreBar_P2Weapons_1_YLoc",
        "ScoreBar_P2Weapons_2_XLoc", "ScoreBar_P2Weapons_2_YLoc",
        "ScoreBar_P2Weapons_3_XLoc", "ScoreBar_P2Weapons_3_YLoc",
        "ScoreBar_Weapons_BlendAmount_NonSelected",
        "ScoreBar_Weapons_BlendAmount_Selected",
        "ScoreBar_Weapons_NonSelectedScaleFactor",
        "ScoreBar_Lives_MaxNumDisplayed"
    }};
    if (game_floats.size() < first_float + labels.size()) {
        fail(error, "Game[gafl] is shorter than the 1.0.6 score-bar positional contract");
        return std::nullopt;
    }
    for (std::size_t i = 0; i < labels.size(); ++i) {
        if (game_floats[first_float + i].first != labels[i]) {
            fail(error, "unexpected Game[gafl] score-bar label at index " + std::to_string(first_float + i));
            return std::nullopt;
        }
    }

    constexpr std::array<std::string_view, 16> rect_labels = {{
        "Scorebar Player 1 Score", "Scorebar Player 1 Life Symbol",
        "Scorebar Player 1 Life Count", "Scorebar Player 1 Weapon 1",
        "Scorebar Player 1 Weapon 2", "Scorebar Player 1 Weapon 3",
        "Scorebar Player 1 Shields", "Scorebar Player 1 Power",
        "Scorebar Player 2 Score", "Scorebar Player 2 Life Symbol",
        "Scorebar Player 2 Life Count", "Scorebar Player 2 Weapon 1",
        "Scorebar Player 2 Weapon 2", "Scorebar Player 2 Weapon 3",
        "Scorebar Player 2 Shields", "Scorebar Player 2 Power"
    }};
    if (game_rects.size() < rect_labels.size()) {
        fail(error, "Rects[inre] is shorter than the 1.0.6 score-bar positional contract");
        return std::nullopt;
    }
    for (std::size_t i = 0; i < rect_labels.size(); ++i) {
        if (game_rects[i].first != rect_labels[i]) {
            fail(error, "unexpected Rects[inre] score-bar label at index " + std::to_string(i));
            return std::nullopt;
        }
    }

    LegacyScoreBarConfig out;
    const auto f = [&](std::size_t i) { return game_floats[first_float + i].second; };
    out.score_spacing = trunc_i(f(0));
    out.lives_symbol_x = {{trunc_i(f(1)), trunc_i(f(3))}};
    out.lives_symbol_y = {{trunc_i(f(2)), trunc_i(f(4))}};
    out.shield_meter_x = {{trunc_i(f(5)), trunc_i(f(7))}};
    out.shield_meter_y = {{trunc_i(f(6)), trunc_i(f(8))}};
    out.shield_increase_rate = f(9);
    out.shield_decrease_rate = f(10);
    out.power_meter_x = {{trunc_i(f(11)), trunc_i(f(13))}};
    out.power_meter_y = {{trunc_i(f(12)), trunc_i(f(14))}};
    out.power_increase_rate = f(15);
    out.power_decrease_rate = f(16);
    out.weapon_x = {{{{trunc_i(f(17)), trunc_i(f(19)), trunc_i(f(21))}},
                     {{trunc_i(f(23)), trunc_i(f(25)), trunc_i(f(27))}}}};
    out.weapon_y = {{{{trunc_i(f(18)), trunc_i(f(20)), trunc_i(f(22))}},
                     {{trunc_i(f(24)), trunc_i(f(26)), trunc_i(f(28))}}}};
    out.weapon_blend_nonselected = trunc_i(f(29));
    out.weapon_blend_selected = trunc_i(f(30));
    out.weapon_nonselected_scale = f(31);
    out.lives_max_displayed = trunc_i(f(32));
    for (std::size_t player = 0; player < 2; ++player) {
        for (std::size_t slot = 0; slot < 8; ++slot) {
            out.panel_rects[player][slot] = game_rects[player * 8 + slot].second;
        }
    }
    return out;
}

LegacyScoreBarWeaponPreview compile_legacy_score_bar_weapon_preview(
    const WeaponDefinition& definition) {
    return LegacyScoreBarWeaponPreview{
        definition.fields.id_value("scoreBarPreviewFace_ID").value_or(FourCC{}),
        definition.fields.int_value("scoreBarPreviewFrame_INT").value_or(0)
    };
}

LegacyScoreBarPlayerResources compile_legacy_score_bar_player_resources(
    const CompiledPlayerRuntimeDefinition& definition) {
    return LegacyScoreBarPlayerResources{
        definition.score_bar_face, definition.score_bar_frame,
        definition.score_bar_power_face, definition.score_bar_power_frame,
        definition.score_bar_shield_face, definition.score_bar_shield_frame
    };
}

LegacyScoreBarPlayerState initialize_legacy_score_bar_player(
    const PlayerRuntimeSlot& player,
    const CompiledPlayerRuntimeDefinition& definition,
    const LegacyScoreBarWeaponInput& weapons) {
    LegacyScoreBarPlayerState out;
    out.resources = compile_legacy_score_bar_player_resources(definition);
    out.cached_score = player.score;
    out.cached_lives = player.lives;
    out.displayed_shield = 0.0f;
    out.displayed_power = 0.0f;
    out.weapon_previews = weapons.previews;
    set_all_dirty(out.dirty, true);
    out.present_latch = player.enabled;
    out.content_visible = player.enabled;
    return out;
}

void advance_legacy_score_bar_player(
    LegacyScoreBarPlayerState& state,
    const PlayerRuntimeSlot& player,
    const LegacyScoreBarWeaponInput& weapons,
    const LegacyScoreBarConfig& config) {
    set_all_dirty(state.dirty, false);

    const bool normal_path = player.enabled && player.status != static_cast<int>(LegacyPlayerStatus::game_over);
    if (normal_path) {
        if (state.cached_score != player.score) {
            state.cached_score = player.score;
            state.dirty.score = true;
        }
        if (state.cached_lives != player.lives) {
            state.cached_lives = player.lives;
            state.dirty.life_count = true;
        }
        if (weapons.previews_changed) {
            state.weapon_previews = weapons.previews;
            state.dirty.weapons = true;
        }

        const bool active = player.status == static_cast<int>(LegacyPlayerStatus::active);
        if (state.displayed_shield != player.shield_percentage) {
            state.dirty.shield = true;
            state.displayed_shield = converge_meter(
                state.displayed_shield, player.shield_percentage,
                config.shield_increase_rate, config.shield_decrease_rate, active);
        }
        if (state.displayed_power != weapons.power_percentage) {
            state.dirty.power = true;
            state.displayed_power = converge_meter(
                state.displayed_power, weapons.power_percentage,
                config.power_increase_rate, config.power_decrease_rate, active);
        }
        // 0x31A38..0x31A64 loads exact PEF constants 0.0 and 100.0.
        state.displayed_power = std::clamp(state.displayed_power, 0.0f, 100.0f);
        return;
    }

    if (state.present_latch) {
        state.content_visible = false;
        set_all_dirty(state.dirty, true);
        state.present_latch = false;
    }
}

int legacy_score_bar_displayed_lives(int semantic_lives, const LegacyScoreBarConfig& config) {
    return std::clamp(semantic_lives - 1, 0, config.lives_max_displayed);
}

} // namespace deimos
