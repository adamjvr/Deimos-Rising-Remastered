#pragma once

#include "deimos/data_tables.hpp"
#include "deimos/player_runtime.hpp"
#include "deimos/weapon_definition.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace deimos {

struct LegacyScoreBarConfig {
    int score_spacing = 13;
    std::array<int, 2> lives_symbol_x{{534, 534}};
    std::array<int, 2> lives_symbol_y{{41, 276}};
    std::array<int, 2> shield_meter_x{{495, 495}};
    std::array<int, 2> shield_meter_y{{124, 359}};
    float shield_increase_rate = 2.0f;
    float shield_decrease_rate = 3.0f;
    std::array<int, 2> power_meter_x{{495, 495}};
    std::array<int, 2> power_meter_y{{159, 394}};
    float power_increase_rate = 2.0f;
    float power_decrease_rate = 4.0f;
    std::array<std::array<int, 3>, 2> weapon_x{{{{467, 502, 530}}, {{467, 502, 530}}}};
    std::array<std::array<int, 3>, 2> weapon_y{{{{199, 199, 199}}, {{434, 434, 434}}}};
    int weapon_blend_nonselected = 16;
    int weapon_blend_selected = 6;
    float weapon_nonselected_scale = 0.7f;
    int lives_max_displayed = 9;
    std::array<std::array<RectI, 8>, 2> panel_rects{};
};

[[nodiscard]] std::optional<LegacyScoreBarConfig> compile_legacy_score_bar_config(
    const NamedTable<float>& game_floats,
    const NamedTable<RectI>& game_rects,
    std::string* error = nullptr);

struct LegacyScoreBarWeaponPreview {
    FourCC face{};
    int frame = 0;
    constexpr bool operator==(const LegacyScoreBarWeaponPreview&) const = default;
};

[[nodiscard]] LegacyScoreBarWeaponPreview compile_legacy_score_bar_weapon_preview(
    const WeaponDefinition& definition);

struct LegacyScoreBarPlayerResources {
    FourCC base_face{};
    int base_frame = 0;
    FourCC power_face{};
    int power_frame = 0;
    FourCC shield_face{};
    int shield_frame = 0;
    constexpr bool operator==(const LegacyScoreBarPlayerResources&) const = default;
};

[[nodiscard]] LegacyScoreBarPlayerResources compile_legacy_score_bar_player_resources(
    const CompiledPlayerRuntimeDefinition& definition);

struct LegacyScoreBarDirty {
    bool score = false;
    bool life_symbol = false;
    bool life_count = false;
    bool weapons = false;
    bool shield = false;
    bool power = false;

    constexpr bool operator==(const LegacyScoreBarDirty&) const = default;
    [[nodiscard]] bool any() const {
        return score || life_symbol || life_count || weapons || shield || power;
    }
};

struct LegacyScoreBarPlayerState {
    LegacyScoreBarPlayerResources resources{};
    int cached_score = 0;
    int cached_lives = 0;
    float displayed_shield = 0.0f;
    float displayed_power = 0.0f;
    std::array<LegacyScoreBarWeaponPreview, 3> weapon_previews{};
    LegacyScoreBarDirty dirty{};
    bool present_latch = false; // original +0x12E
    bool content_visible = false; // original +0x12F
};

struct LegacyScoreBarWeaponInput {
    float power_percentage = 0.0f;
    bool previews_changed = false;
    std::array<LegacyScoreBarWeaponPreview, 3> previews{};
};

// PPC 0x31400: cache score/lives and previews, start meters at zero, mark all
// six dirty classes, and latch player-enabled state.
[[nodiscard]] LegacyScoreBarPlayerState initialize_legacy_score_bar_player(
    const PlayerRuntimeSlot& player,
    const CompiledPlayerRuntimeDefinition& definition,
    const LegacyScoreBarWeaponInput& weapons = {});

// PPC 0x317E0. The normal update path runs only for enabled players that have
// not entered status 1 (game over). Shield/power decreases always converge;
// increases occur only while status 4 is active. Power is clamped to the exact
// binary constants 0.0..100.0 after convergence.
void advance_legacy_score_bar_player(
    LegacyScoreBarPlayerState& state,
    const PlayerRuntimeSlot& player,
    const LegacyScoreBarWeaponInput& weapons,
    const LegacyScoreBarConfig& config);

[[nodiscard]] int legacy_score_bar_displayed_lives(
    int semantic_lives,
    const LegacyScoreBarConfig& config);

} // namespace deimos
