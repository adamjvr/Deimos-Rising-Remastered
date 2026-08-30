#include "deimos/score_bar_runtime.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string_view>
#include <vector>

namespace deimos {
namespace {

constexpr FourCC fourcc(char a, char b, char c, char d) {
    return FourCC{{a, b, c, d}};
}

bool absent_preview(FourCC id) {
    return id == FourCC{} || id == fourcc('n', 'o', 'n', 'e');
}

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

int half_fade_toward_32(int blend) {
    const int delta = 32 - blend;
    // PPC sequence implements signed division by two rounded toward zero.
    const int half = delta / 2;
    return std::min(32, blend + half);
}

LegacyRasterRect translated_panel_rect(const RectI& r) {
    constexpr int kScoreBarCanvasX = 416;
    return {r.top, r.left + kScoreBarCanvasX, r.bottom, r.right + kScoreBarCanvasX};
}

bool restore_panel_region(const LegacyRasterSurface& base, const RectI& local,
                          LegacyRasterSurface& canvas) {
    if (!base.valid() || base.width != 160 || base.height != 480 || !canvas.valid()) return false;
    for (int y = std::max(0, local.top); y < std::min(base.height, local.bottom); ++y) {
        for (int x = std::max(0, local.left); x < std::min(base.width, local.right); ++x) {
            const int dx = x + 416;
            if (dx < 0 || dx >= canvas.width || y < 0 || y >= canvas.height) continue;
            canvas.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(canvas.width) + static_cast<std::size_t>(dx)] =
                base.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(base.width) + static_cast<std::size_t>(x)];
        }
    }
    return true;
}

bool draw_sprite(const LegacySpriteCache& cache, FourCC face, int frame,
                 int x, int y, LegacyRasterSurface& canvas,
                 std::uint32_t flags = 0, float scale = 1.0f, int amount = 0) {
    const auto* f = cache.find_loaded_frame(face, frame);
    if (!f) return false;
    LegacyRasterRequest q;
    q.frame = f;
    q.center_x = x;
    q.center_y = y;
    q.sprite_face = face;
    q.sprite_frame = frame;
    q.flags = flags;
    q.scale = scale;
    q.effect_amount_0_to_32 = amount;
    q.clip = canvas.bounds();
    q.immediate = true;
    return rasterize_legacy_request(q, canvas) != LegacyRasterResult::invalid_surface;
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


std::optional<int> legacy_small_text_frame_for_char(unsigned char c) {
    if (c == ' ') return std::nullopt;
    if (c >= 'A' && c <= 'Z') return static_cast<int>(c - 'A');
    if (c >= 'a' && c <= 'z') return 26 + static_cast<int>(c - 'a');
    if (c >= '1' && c <= '9') return 52 + static_cast<int>(c - '1');
    if (c == '0') return 61;
    switch (c) {
    case '!': return 62; case '"': return 63; case '#': return 64; case '$': return 65;
    case '%': return 66; case '&': return 67; case '\'': return 68;
    case '(': case '[': case '{': return 69;
    case ')': case ']': case '}': return 70;
    case '*': return 71; case '+': return 72; case ',': return 73; case '-': return 74;
    case '.': return 75; case '/': return 76; case ':': return 77; case ';': return 78;
    case '<': return 79; case '=': return 80; case '>': return 81; case '?': return 82;
    case '@': return 83; case '\\': return 84; case '^': return 85; case '_': return 86;
    case '`': return 87; case '|': return 88; case '~': return 89;
    default: return std::nullopt;
    }
}

std::vector<LegacyRasterRequest> build_legacy_small_text_requests(
    const LegacySpriteGroupMetadata& font,
    const TextFormatDefinition& style,
    std::string_view text,
    bool hidden_content) {
    std::vector<LegacyRasterRequest> out;
    if (font.frames.empty()) return out;

    int mono_w = 0;
    for (unsigned char d = '0'; d <= '9'; ++d) {
        const auto fi = legacy_small_text_frame_for_char(d);
        if (!fi || *fi < 0 || static_cast<std::size_t>(*fi) >= font.frames.size()) continue;
        mono_w = std::max(mono_w, font.frames[*fi].width);
    }

    float total = 0.0f;
    for (unsigned char c : text) {
        int metric_w = 0;
        if (style.monospaced) {
            metric_w = mono_w;
        } else if (const auto fi = legacy_small_text_frame_for_char(c);
                   fi && *fi >= 0 && static_cast<std::size_t>(*fi) < font.frames.size()) {
            metric_w = font.frames[*fi].width;
        }
        total += static_cast<float>(metric_w + style.space_between_chars);
    }

    int x = style.x;
    if (style.format_token == "CENT") {
        x = static_cast<int>(std::trunc(static_cast<float>(style.x) - total * 0.5f));
    } else if (style.format_token == "RIGH") {
        x = static_cast<int>(std::trunc(static_cast<float>(style.x) - total));
    }

    int blend = style.blend_amount_0_to_32;
    if (hidden_content) blend = half_fade_toward_32(blend);

    for (unsigned char c : text) {
        x += style.space_between_chars; // 0xE670 advances spacing before each glyph.
        const auto fi = legacy_small_text_frame_for_char(c);
        const int metric_w = style.monospaced ? mono_w :
            (fi && *fi >= 0 && static_cast<std::size_t>(*fi) < font.frames.size() ? font.frames[*fi].width : 0);
        if (fi && *fi >= 0 && static_cast<std::size_t>(*fi) < font.frames.size()) {
            const auto& frame = font.frames[*fi];
            LegacyRasterRequest q;
            q.frame = &frame;
            q.center_x = x + frame.width / 2;
            q.center_y = style.y + frame.height / 2;
            q.sprite_face = font.id;
            q.sprite_frame = *fi;
            q.effect_amount_0_to_32 = blend;
            q.scale = 1.0f;
            q.immediate = true;
            if (style.colorise) {
                q.flags |= kLegacyRenderSolidColor;
                q.effect_color = legacy_rgb24_to_rgb555(style.colorise_color);
            } else if (blend != 0) {
                q.flags |= kLegacyRenderOverallTransparency;
            }
            out.push_back(q);
        }
        x += metric_w;
    }
    return out;
}

std::string format_legacy_score_value(int score) {
    char buf[64]{};
    std::snprintf(buf, sizeof(buf), "%.7i", score);
    return buf;
}

std::string format_legacy_lives_value(int displayed_lives) {
    char buf[64]{};
    std::snprintf(buf, sizeof(buf), "%i", displayed_lives);
    return buf;
}

bool rasterize_legacy_score_bar_player(
    int player_index,
    const LegacyScoreBarPlayerState& state,
    const LegacyScoreBarConfig& config,
    const LegacyScoreBarTextStyles& styles,
    const LegacyScoreBarRenderAssets& assets,
    LegacyRasterSurface& canvas,
    std::string* error) {
    if (player_index < 0 || player_index > 1) {
        fail(error, "score-bar player index is outside 0..1");
        return false;
    }
    if (!assets.base_panel || !assets.base_panel->valid() ||
        assets.base_panel->width != 160 || assets.base_panel->height != 480) {
        fail(error, "score-bar base panel is not the canonical 160x480 surface");
        return false;
    }
    if (!assets.small_text_font || assets.small_text_font->frames.size() < 90) {
        fail(error, "score-bar small-text font is missing or incomplete");
        return false;
    }
    if (!assets.sprites) {
        fail(error, "score-bar sprite cache is missing");
        return false;
    }
    if (!canvas.valid() || canvas.width < 576 || canvas.height < 480) {
        fail(error, "score-bar destination canvas is smaller than 576x480");
        return false;
    }

    const auto restore = [&](int slot) {
        return restore_panel_region(*assets.base_panel, config.panel_rects[player_index][slot], canvas);
    };
    const auto draw_text = [&](const TextFormatDefinition& style, std::string_view text, bool hidden) {
        auto reqs = build_legacy_small_text_requests(*assets.small_text_font, style, text, hidden);
        for (auto& q : reqs) {
            q.clip = canvas.bounds();
            if (rasterize_legacy_request(q, canvas) == LegacyRasterResult::invalid_surface) return false;
        }
        return true;
    };

    if (state.dirty.score) {
        if (!restore(0) || !draw_text(styles.score[player_index], format_legacy_score_value(state.cached_score), !state.content_visible)) return false;
    }

    if (state.dirty.life_symbol) {
        if (!restore(1)) return false;
        std::uint32_t flags = 0;
        int amount = 0;
        if (!state.content_visible) { flags |= kLegacyRenderOverallTransparency; amount = 16; }
        if (!draw_sprite(*assets.sprites, state.resources.base_face, state.resources.base_frame,
                         config.lives_symbol_x[player_index], config.lives_symbol_y[player_index],
                         canvas, flags, 1.0f, amount)) {
            fail(error, "score-bar life-symbol frame is missing");
            return false;
        }
    }

    if (state.dirty.life_count) {
        if (!restore(2)) return false;
        const int displayed = legacy_score_bar_displayed_lives(state.cached_lives, config);
        const auto& style = (state.content_visible && displayed == 0)
            ? styles.last_life[player_index] : styles.lives[player_index];
        if (!draw_text(style, format_legacy_lives_value(displayed), !state.content_visible)) return false;
    }

    if (state.dirty.weapons) {
        for (int slot = 0; slot < 3; ++slot) {
            if (!restore(3 + slot)) return false;
            if (!state.content_visible) continue; // dispatcher restores but suppresses preview draw.
            const auto& preview = state.weapon_previews[slot];
            // Empty/unavailable live weapon slots are restored to the original
            // score-panel background and intentionally draw no preview. The
            // old integration treated `none` as a missing required sprite and
            // therefore could not represent Level-1's locked weapon slots.
            if (absent_preview(preview.face)) continue;
            const bool selected = slot == 0;
            const int amount = selected ? config.weapon_blend_selected : config.weapon_blend_nonselected;
            const float scale = selected ? 1.0f : config.weapon_nonselected_scale;
            if (!draw_sprite(*assets.sprites, preview.face, preview.frame,
                             config.weapon_x[player_index][slot], config.weapon_y[player_index][slot],
                             canvas, kLegacyRenderOverallTransparency, scale, amount)) {
                fail(error, "score-bar weapon-preview frame is missing");
                return false;
            }
        }
    }

    const auto draw_meter = [&](int slot, FourCC face, int frame, int x, int y,
                                float percentage, const TextFormatDefinition& style) {
        if (!restore(slot)) return false;
        if (!draw_sprite(*assets.sprites, face, frame, x, y, canvas)) return false;
        const float pct = std::clamp(percentage, 0.0f, 100.0f);
        if (pct < 100.0f) {
            const auto full = translated_panel_rect(config.panel_rects[player_index][slot]);
            const int width = full.right - full.left;
            const int fill = static_cast<int>(std::trunc((pct / 100.0f) * static_cast<float>(width)));
            LegacyRasterRect strip = full;
            strip.right = strip.left + fill;
            if (!strip.empty()) {
                const auto result = rasterize_legacy_solid_rect(
                    canvas, strip, canvas.bounds(), legacy_rgb24_to_rgb555(style.color_strip_color),
                    style.color_strip_blend_amount_0_to_32);
                if (result == LegacyRasterResult::invalid_surface) return false;
            }
        }
        return true;
    };

    if (state.dirty.shield && !draw_meter(6, state.resources.shield_face, state.resources.shield_frame,
                                          config.shield_meter_x[player_index], config.shield_meter_y[player_index],
                                          state.displayed_shield, styles.shield)) {
        fail(error, "score-bar shield meter render failed");
        return false;
    }
    if (state.dirty.power && !draw_meter(7, state.resources.power_face, state.resources.power_frame,
                                         config.power_meter_x[player_index], config.power_meter_y[player_index],
                                         state.displayed_power, styles.power)) {
        fail(error, "score-bar power meter render failed");
        return false;
    }
    return true;
}

} // namespace deimos
