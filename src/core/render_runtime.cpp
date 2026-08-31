#include "deimos/render_runtime.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace deimos {
namespace {
constexpr FourCC fourcc(char a, char b, char c, char d) {
    return FourCC{{a, b, c, d}};
}

bool absent_face(FourCC id) {
    // Entity world draw loops explicitly reject only the 'none' sentinel. A
    // zero/other face is not silently normalized by that outer PPC check.
    return id == fourcc('n', 'o', 'n', 'e');
}

float move_percent_toward(float current, float target, float delta, bool clamp_zero) {
    if (current > target) {
        current -= delta;
        if (clamp_zero && current < 0.0f) current = 0.0f;
        if (current < target) current = target;
    } else if (current < target) {
        current += delta;
        if (current > target) current = target;
    }
    return current;
}

LegacyRenderIntent base_intent(
    const LegacySpriteVisualRuntime& runtime,
    LegacyRenderPassKind kind) {
    LegacyRenderIntent intent;
    intent.kind = kind;
    intent.sprite_face = runtime.sprite_face;
    intent.sprite_frame = runtime.sprite_frame;
    intent.numeric_layer = kind == LegacyRenderPassKind::shadow
        ? legacy_shadow_layer_code(runtime.draw_layer, runtime.air_domain)
        : legacy_draw_layer_code(runtime.draw_layer, runtime.air_domain);
    if (runtime.draw_to_terrain) {
        // 0x12FA0 main terrain submissions are queued on one-shot layer 1;
        // 0x13460 shadow terrain submissions use companion one-shot layer 0.
        intent.numeric_layer = (kind == LegacyRenderPassKind::shadow) ? 0 : 1;
    }
    intent.scale = runtime.scale;
    intent.visibility_percent = runtime.visibility_percent;
    intent.draw_to_terrain = runtime.draw_to_terrain;
    intent.world_space = runtime.world_space;
    return intent;
}
} // namespace

float legacy_percent_to_scale(int percent) {
    return static_cast<float>(percent) / 100.0f;
}

int legacy_percent_to_transparency_0_to_32(float percent) {
    // 0x13580 and the main-sprite visibility path fctiwz before 0x10C20.
    const int truncated_percent = static_cast<int>(std::trunc(percent));
    const float mapped = std::fabs(
        (static_cast<float>(truncated_percent) / 100.0f) * 32.0f - 32.0f);
    int transparency = static_cast<int>(std::trunc(mapped));
    if (transparency > 32) transparency = 32;
    return transparency;
}

int legacy_tint_effect_transparency_0_to_32(
    float tint_percent,
    float visibility_percent) {
    const int visibility_transparency =
        legacy_percent_to_transparency_0_to_32(visibility_percent);
    const float tint_fraction = tint_percent / 100.0f;
    const float raw_tint_coverage = 32.0f * tint_fraction;
    const float visible_fraction =
        1.0f - (static_cast<float>(visibility_transparency) / 32.0f);
    const float effective_coverage = raw_tint_coverage * visible_fraction;
    const float mapped = std::fabs(effective_coverage - 32.0f);
    int transparency = static_cast<int>(std::trunc(mapped));
    if (transparency > 32) transparency = 32;
    return transparency;
}

LegacySpriteVisualRuntime initialise_legacy_sprite_visual(
    const CompiledUnitBehavior& behavior,
    std::size_t state_index,
    LegacyRandom& random,
    int selected_frame) {
    if (state_index >= behavior.states.size()) {
        throw std::out_of_range("visual state index outside compiled behavior");
    }

    const auto& state = behavior.states[state_index];
    LegacySpriteVisualRuntime out;
    out.sprite_face = state.sprite_face;
    out.sprite_frame = selected_frame;
    out.draw_layer = behavior.draw_layer;
    out.air_domain = behavior.collision_domain == fourcc('a', 'i', 'r', ' ');
    out.world_space = behavior.draw_layer != fourcc('h', 'u', 'd', ' ');
    out.casts_shadows = behavior.casts_shadows;
    out.adjust_shadow_location_for_scaling = behavior.adjust_shadow_location_for_scaling;
    out.draw_to_terrain = state.draw_to_terrain;
    out.do_colorise = state.do_colorise;

    out.visibility_percent = static_cast<float>(behavior.initial_visibility_percent);
    out.required_visibility_percent = static_cast<float>(state.required_visibility_percent);
    out.visibility_delta_percent = static_cast<float>(state.visibility_delta_percent);

    // The initial state-entry path seeds current tint from the state's tint,
    // rather than from a separate Unit Definition default.
    out.tint_percent = static_cast<float>(state.tint_percent);
    out.required_tint_percent = static_cast<float>(state.tint_percent);
    out.tint_delta_percent = static_cast<float>(state.tint_delta_percent);
    out.tint_color = state.tint_color;

    int initial_scale = behavior.initial_scale_percent;
    if (behavior.initial_scale_tolerance_percent != 0) {
        // PPC signed division by two truncates toward zero.
        const int half = behavior.initial_scale_tolerance_percent / 2;
        initial_scale += choose_inclusive_integer(-half, half, random);
        if (initial_scale < 0) initial_scale = 0;
    }
    out.scale = legacy_percent_to_scale(initial_scale);
    out.required_scale = legacy_percent_to_scale(state.required_scale_percent);
    out.scale_delta = legacy_percent_to_scale(state.scale_delta_percent);
    out.bounds_dirty = true;
    return out;
}

void apply_legacy_state_visual_targets(
    LegacySpriteVisualRuntime& runtime,
    const CompiledUnitStateBehavior& state,
    int selected_frame) {
    const bool geometry_source_changed =
        runtime.sprite_face != state.sprite_face || runtime.sprite_frame != selected_frame;
    runtime.sprite_face = state.sprite_face;
    runtime.sprite_frame = selected_frame;
    runtime.draw_to_terrain = state.draw_to_terrain;
    runtime.do_colorise = state.do_colorise;
    runtime.required_visibility_percent = static_cast<float>(state.required_visibility_percent);
    runtime.visibility_delta_percent = static_cast<float>(state.visibility_delta_percent);
    runtime.required_tint_percent = static_cast<float>(state.tint_percent);
    runtime.tint_delta_percent = static_cast<float>(state.tint_delta_percent);
    runtime.tint_color = state.tint_color;
    runtime.required_scale = legacy_percent_to_scale(state.required_scale_percent);
    runtime.scale_delta = legacy_percent_to_scale(state.scale_delta_percent);
    if (geometry_source_changed) runtime.bounds_dirty = true;
}

LegacyVisualTickResult tick_legacy_visual_scalars(LegacySpriteVisualRuntime& runtime) {
    LegacyVisualTickResult result;

    const float old_visibility = runtime.visibility_percent;
    runtime.visibility_percent = move_percent_toward(
        runtime.visibility_percent,
        runtime.required_visibility_percent,
        runtime.visibility_delta_percent,
        true);
    result.visibility_changed = runtime.visibility_percent != old_visibility;

    const float old_tint = runtime.tint_percent;
    runtime.tint_percent = move_percent_toward(
        runtime.tint_percent,
        runtime.required_tint_percent,
        runtime.tint_delta_percent,
        true);
    result.tint_changed = runtime.tint_percent != old_tint;

    const float old_scale = runtime.scale;
    runtime.scale = move_percent_toward(
        runtime.scale,
        runtime.required_scale,
        runtime.scale_delta,
        false);
    result.scale_changed = runtime.scale != old_scale;
    if (result.scale_changed) runtime.bounds_dirty = true;

    return result;
}

void trigger_legacy_collision_glow(
    LegacySpriteVisualRuntime& runtime,
    Rgb24 color,
    int step,
    bool restart) {
    // PPC 0x12BC0 returns early when a pulse is already active and r6==0.
    if (runtime.collision_glow_active && !restart) return;

    runtime.collision_glow_active = true;
    runtime.collision_glow_amount_0_to_32 = 32;
    runtime.collision_glow_toward_peak = true;
    runtime.collision_glow_step = std::max(step, 0);
    runtime.collision_glow_color = color;
}

void tick_legacy_collision_glow(LegacySpriteVisualRuntime& runtime) {
    if (!runtime.collision_glow_active) return;

    if (runtime.collision_glow_toward_peak) {
        runtime.collision_glow_amount_0_to_32 -= runtime.collision_glow_step;
        if (runtime.collision_glow_amount_0_to_32 > 4) return;
        runtime.collision_glow_amount_0_to_32 = 4;
        runtime.collision_glow_toward_peak = false;
        return;
    }

    runtime.collision_glow_amount_0_to_32 += runtime.collision_glow_step;
    if (runtime.collision_glow_amount_0_to_32 < 32) return;
    runtime.collision_glow_amount_0_to_32 = 32;
    runtime.collision_glow_toward_peak = true;
    runtime.collision_glow_active = false;
}

bool refresh_legacy_sprite_geometry(
    LegacySpriteVisualRuntime& runtime,
    LegacySpriteCache& cache,
    const LegacySpriteCache::Loader& loader) {
    if (!runtime.bounds_dirty) return true;

    if (runtime.sprite_face == fourcc('n', 'o', 'n', 'e')) {
        runtime.half_width = 0;
        runtime.half_height = 0;
        runtime.bounds_dirty = false;
        return true;
    }

    // 0x12940 prefers live +0x50 when already cached; the clean cache lookup
    // yields the same metadata result and only invokes the loader if the whole
    // group is absent, matching the normal 0x19CA0 path.
    const auto* already_loaded = cache.find_loaded_frame(runtime.sprite_face, runtime.sprite_frame);
    std::pair<int, int> dimensions{};
    if (already_loaded) {
        dimensions = legacy_scaled_sprite_dimensions(*already_loaded, runtime.scale);
    } else {
        dimensions = cache.dimensions(runtime.sprite_face, runtime.sprite_frame, runtime.scale, loader);
        if (!cache.find_loaded_frame(runtime.sprite_face, runtime.sprite_frame)) return false;
    }

    runtime.sprite_width = dimensions.first;
    runtime.sprite_height = dimensions.second;
    // C++ signed integer division truncates toward zero, matching the PPC
    // sign-adjust + srawi sequence used for each half extent.
    runtime.half_width = runtime.sprite_width / 2;
    runtime.half_height = runtime.sprite_height / 2;
    runtime.bounds_dirty = false;
    return true;
}

int legacy_draw_layer_code(FourCC draw_layer, bool air_domain) {
    // 0x130C4..0x130E4 mutates zero/none to 'defa' before the switch.
    if (draw_layer == FourCC{} || draw_layer == fourcc('n', 'o', 'n', 'e')) {
        draw_layer = fourcc('d', 'e', 'f', 'a');
    }
    if (draw_layer == fourcc('d', 'e', 'f', 'a')) return air_domain ? 7 : 3;
    if (draw_layer == fourcc('g', 'r', 'o', 'u')) return 3;
    if (draw_layer == fourcc('g', 'r', 'h', 'i')) return 5;
    if (draw_layer == fourcc('a', 'i', 'l', 'o')) return 7;
    if (draw_layer == fourcc('a', 'i', 'h', 'i')) return 8;
    if (draw_layer == fourcc('p', 'l', 'w', 'e')) return 9;
    if (draw_layer == fourcc('p', 'l', 'a', 'y')) return 10;
    if (draw_layer == fourcc('p', 'l', 's', 'h')) return 11;
    if (draw_layer == fourcc('p', 'l', 'e', 'f')) return 12;
    if (draw_layer == fourcc('p', 'l', 'u', 'i')) return 13;
    if (draw_layer == fourcc('a', 't', 'm', 'o')) return 14;
    if (draw_layer == fourcc('h', 'u', 'd', ' ')) return 15;
    return 0;
}

namespace {
bool shadow_uses_air_offsets(FourCC draw_layer, bool air_domain) {
    if (draw_layer == FourCC{} || draw_layer == fourcc('n', 'o', 'n', 'e')) {
        draw_layer = fourcc('d', 'e', 'f', 'a');
    }
    if (draw_layer == fourcc('d', 'e', 'f', 'a')) return air_domain;
    if (draw_layer == fourcc('g', 'r', 'o', 'u') || draw_layer == fourcc('g', 'r', 'h', 'i')) return false;
    if (draw_layer == fourcc('a', 'i', 'l', 'o') ||
        draw_layer == fourcc('a', 'i', 'h', 'i') ||
        draw_layer == fourcc('p', 'l', 'w', 'e') ||
        draw_layer == fourcc('p', 'l', 'a', 'y') ||
        draw_layer == fourcc('p', 'l', 's', 'h') ||
        draw_layer == fourcc('p', 'l', 'e', 'f') ||
        draw_layer == fourcc('p', 'l', 'u', 'i') ||
        draw_layer == fourcc('a', 't', 'm', 'o') ||
        draw_layer == fourcc('h', 'u', 'd', ' ')) return true;
    // 0x13ED0 keeps the offsets prepared from +0x19 for unknown layers.
    return air_domain;
}

int legacy_shadow_transparency(float visibility_percent) {
    int transparency = legacy_percent_to_transparency_0_to_32(visibility_percent);
    // 0x135A0..0x135A8 prevents shadows from becoming more opaque than 12/32.
    if (transparency < 20) transparency = 20;
    return transparency;
}
}

int legacy_shadow_layer_code(FourCC draw_layer, bool air_domain) {
    // 0x135B0..0x13D7C uses the companion shadow layer domain: default
    // ground/air are 2/6, explicit ground-high is 4, and the remaining
    // recognized non-ground layers use the air-shadow layer 6.
    if (draw_layer == FourCC{} || draw_layer == fourcc('n', 'o', 'n', 'e')) {
        draw_layer = fourcc('d', 'e', 'f', 'a');
    }
    if (draw_layer == fourcc('d', 'e', 'f', 'a')) return air_domain ? 6 : 2;
    if (draw_layer == fourcc('g', 'r', 'o', 'u')) return 2;
    if (draw_layer == fourcc('g', 'r', 'h', 'i')) return 4;
    if (draw_layer == fourcc('a', 'i', 'l', 'o') ||
        draw_layer == fourcc('a', 'i', 'h', 'i') ||
        draw_layer == fourcc('p', 'l', 'w', 'e') ||
        draw_layer == fourcc('p', 'l', 'a', 'y') ||
        draw_layer == fourcc('p', 'l', 's', 'h') ||
        draw_layer == fourcc('p', 'l', 'e', 'f') ||
        draw_layer == fourcc('p', 'l', 'u', 'i') ||
        draw_layer == fourcc('a', 't', 'm', 'o') ||
        draw_layer == fourcc('h', 'u', 'd', ' ')) return 6;
    return 0;
}

std::optional<LegacyShadowRuntimeConfig> compile_legacy_shadow_runtime_config(
    const NamedTable<float>& game_floats,
    std::string* error) {
    constexpr std::size_t first = 48;
    constexpr std::array<const char*, 4> labels = {{
        "Shadow_XOffset", "Shadow_YOffset", "Shadow_GroundXOffset", "Shadow_GroundYOffset"
    }};
    if (game_floats.size() < first + labels.size()) {
        if (error) *error = "Game[gafl] is shorter than the 1.0.6 shadow positional contract";
        return std::nullopt;
    }
    for (std::size_t i = 0; i < labels.size(); ++i) {
        if (game_floats[first + i].first != labels[i]) {
            if (error) *error = "unexpected Game[gafl] shadow label at index " + std::to_string(first + i);
            return std::nullopt;
        }
    }
    LegacyShadowRuntimeConfig out;
    out.air_x_offset = game_floats[48].second;
    out.air_y_offset = game_floats[49].second;
    out.ground_x_offset = game_floats[50].second;
    out.ground_y_offset = game_floats[51].second;
    return out;
}

LegacyShadowRequestGeometry build_legacy_shadow_request_geometry(
    const LegacySpriteVisualRuntime& runtime,
    float world_x,
    float world_y,
    const LegacyShadowRuntimeConfig& config,
    LegacyShadowTransformContext context) {
    LegacyShadowRequestGeometry out;
    out.numeric_layer = runtime.draw_to_terrain ? 0 : legacy_shadow_layer_code(runtime.draw_layer, runtime.air_domain);
    out.transparency_0_to_32 = legacy_shadow_transparency(runtime.visibility_percent);
    out.draw_to_terrain = runtime.draw_to_terrain;

    const bool air_offsets = shadow_uses_air_offsets(runtime.draw_layer, runtime.air_domain);
    float offset_scale = runtime.scale;
    int raw_x = 0;
    int raw_y = 0;
    if (air_offsets) {
        out.scale = 0.5f * runtime.scale; // code literal table +0x18 == 0.5f
        offset_scale = runtime.adjust_shadow_location_for_scaling ? out.scale : 0.5f;
        raw_x = static_cast<int>(std::trunc(config.air_x_offset));
        raw_y = static_cast<int>(std::trunc(config.air_y_offset));
    } else {
        out.scale = runtime.scale;
        raw_x = static_cast<int>(std::trunc(config.ground_x_offset));
        raw_y = static_cast<int>(std::trunc(config.ground_y_offset));
    }
    const int x_offset = static_cast<int>(std::trunc(static_cast<float>(raw_x) * offset_scale));
    const int y_offset = static_cast<int>(std::trunc(static_cast<float>(raw_y) * offset_scale));

    if (runtime.draw_to_terrain) {
        // 0x13490/0x1387C and companion branches use a fixed 32-pixel X shift
        // and the current 0xFEC0 world/background Y origin.
        out.x = static_cast<int>(std::trunc(world_x + static_cast<float>(x_offset) - 32.0f));
        out.y = static_cast<int>(std::trunc(world_y + static_cast<float>(y_offset) +
                                            static_cast<float>(context.world_y_origin)));
    } else {
        const int view_offset = runtime.world_space ? context.horizontal_view_offset : 0;
        out.x = static_cast<int>(std::trunc(world_x + static_cast<float>(x_offset) -
                                            static_cast<float>(view_offset)));
        out.y = static_cast<int>(std::trunc(world_y + static_cast<float>(y_offset)));
    }
    return out;
}

std::vector<LegacyRenderIntent> build_legacy_render_intents(
    const LegacySpriteVisualRuntime& runtime,
    LegacyRenderPassSelection selection) {
    std::vector<LegacyRenderIntent> out;
    if (runtime.visibility_percent <= 0.0f || absent_face(runtime.sprite_face)) return out;

    // The world orchestration only selects the shadow pass for shadow-casting
    // entities; 0x12F20 then applies the user/global shadow gate before calling
    // 0x13460. Preserve that two-level eligibility without modeling pixels.
    if (selection.shadow && selection.global_shadows_enabled && runtime.casts_shadows) {
        auto intent = base_intent(runtime, LegacyRenderPassKind::shadow);
        intent.effect_amount_0_to_32 = legacy_shadow_transparency(runtime.visibility_percent);
        out.push_back(intent);
    }

    if (!selection.main) return out;

    // 0x12FA0 suppresses the ordinary base submission when stateDoColorise is
    // set, then independently emits tint and collision-glow effect passes.
    if (!runtime.do_colorise) {
        auto intent = base_intent(runtime, LegacyRenderPassKind::base_sprite);
        intent.effect_amount_0_to_32 =
            legacy_percent_to_transparency_0_to_32(runtime.visibility_percent);
        out.push_back(intent);
    }
    if (runtime.tint_percent > 0.0f) {
        auto intent = base_intent(runtime, LegacyRenderPassKind::tint);
        intent.effect_amount_0_to_32 = legacy_tint_effect_transparency_0_to_32(
            runtime.tint_percent, runtime.visibility_percent);
        intent.effect_color = runtime.tint_color;
        out.push_back(intent);
    }
    if (runtime.collision_glow_active) {
        auto intent = base_intent(runtime, LegacyRenderPassKind::collision_glow);
        intent.effect_amount_0_to_32 = runtime.collision_glow_amount_0_to_32;
        intent.effect_color = runtime.collision_glow_color;
        out.push_back(intent);
    }
    return out;
}


bool legacy_terrain_submission_due(
    LegacySpriteVisualRuntime& runtime,
    std::uint32_t current_sequence) {
    if (!runtime.draw_to_terrain) return false;
    if (current_sequence <= runtime.last_terrain_submit_sequence) return false;
    runtime.last_terrain_submit_sequence = current_sequence;
    return true;
}

} // namespace deimos
