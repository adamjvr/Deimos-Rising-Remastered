#pragma once

#include "deimos/data_tables.hpp"
#include "deimos/player_runtime.hpp"
#include "deimos/render_backend.hpp"
#include "deimos/sprite_resource.hpp"
#include "deimos/weapon_definition.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

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


// Text-format slots used directly by 0x31D70/0x32050 and the meter COST
// overlays. The original loader resolves `tesp` -> `tesm`; the latter lives in
// Interface.pak, not Game.pak.
struct LegacyScoreBarTextStyles {
    TextFormatDefinition shield{};             // runtime ordinal 41 / sbsh
    TextFormatDefinition power{};              // runtime ordinal 42 / sbpm
    std::array<TextFormatDefinition, 2> score{};      // 43/44 / sbs1,sbs2
    std::array<TextFormatDefinition, 2> lives{};      // 45/46 / sbl1,sbl2
    std::array<TextFormatDefinition, 2> last_life{};  // 47/48 / sll1,sll2
};

// Exact printable-ASCII dispatch reconstructed from 0xE8D0. Space is a
// layout-only character and returns no frame. Unsupported bytes return null.
[[nodiscard]] std::optional<int> legacy_small_text_frame_for_char(unsigned char c);

// Upper five bits per RGB24 channel, matching the xRGB1555 words used by the
// original compositor and text Colorise_RGB fields.
[[nodiscard]] std::uint16_t legacy_rgb24_to_rgb555(Rgb24 color);

// Build the direct 0x19570 requests produced by the original small-text path.
// For the score/lives formats used here DrawShadows is false; the function also
// supports CENTER/LEFT/RIGHT alignment, monospaced metrics and the recovered
// pre-glyph spacing convention. hidden_content applies the score-bar half-fade
// toward transparency 32 before requests are emitted.
[[nodiscard]] std::vector<LegacyRasterRequest> build_legacy_small_text_requests(
    const LegacySpriteGroupMetadata& small_text_font,
    const TextFormatDefinition& style,
    std::string_view text,
    bool hidden_content = false);

// Exact C formatting used by 0x31D70 and 0x32050.
[[nodiscard]] std::string format_legacy_score_value(int score); // "%0.7i"
[[nodiscard]] std::string format_legacy_lives_value(int displayed_lives); // "%i"

struct LegacyScoreBarRenderAssets {
    const LegacyRasterSurface* base_panel = nullptr; // canonical `scor` 160x480 TGA
    const LegacySpriteGroupMetadata* small_text_font = nullptr; // canonical `tesm`
    const LegacySpriteCache* sprites = nullptr;
};

// Rasterize the six dirty classes for one player exactly into the 576x480
// source-presentation canvas. The static panel occupies x=416..575; Rects[inre]
// are local to that 160px panel and are translated by +416 before restore/draw.
// Dirty flags are inputs only and are not cleared here.
[[nodiscard]] bool rasterize_legacy_score_bar_player(
    int player_index,
    const LegacyScoreBarPlayerState& state,
    const LegacyScoreBarConfig& config,
    const LegacyScoreBarTextStyles& styles,
    const LegacyScoreBarRenderAssets& assets,
    LegacyRasterSurface& presentation_canvas,
    std::string* error = nullptr);

} // namespace deimos
