#pragma once

#include "deimos/data_tables.hpp"
#include "deimos/state_runtime.hpp"
#include "deimos/sprite_resource.hpp"
#include "deimos/unit_behavior.hpp"

#include <cstddef>
#include <vector>

namespace deimos {

// The 1.0.6 engine keeps visibility/tint as percentage-domain floats, while
// scale is converted from an integer percentage into a factor by the helper at
// 0x1A260. Keeping those domains distinct prevents accidental modernisation of
// the original ramp comparisons.
struct LegacySpriteVisualRuntime {
    FourCC sprite_face{};
    int sprite_frame = 0;
    FourCC draw_layer{};

    bool air_domain = false;
    bool world_space = true;
    bool casts_shadows = false;
    bool adjust_shadow_location_for_scaling = false;
    bool draw_to_terrain = false;
    bool do_colorise = false;

    float visibility_percent = 0.0f;
    float required_visibility_percent = 0.0f;
    float visibility_delta_percent = 0.0f;

    float tint_percent = 0.0f;
    float required_tint_percent = 0.0f;
    float tint_delta_percent = 0.0f;
    Rgb24 tint_color{};

    float scale = 0.0f;
    float required_scale = 0.0f;
    float scale_delta = 0.0f;

    int sprite_width = 0;
    int sprite_height = 0;
    int half_width = 0;
    int half_height = 0;
    bool bounds_dirty = true;

    // 0x12F20 uses temporary main/shadow gates while the world renderer runs
    // separate sorted passes. They are modeled as call-time selection below,
    // not persistent entity properties.
    bool collision_glow_active = false;
    float collision_glow_amount = 0.0f;
    Rgb24 collision_glow_color{};
};

struct LegacyVisualTickResult {
    bool visibility_changed = false;
    bool tint_changed = false;
    bool scale_changed = false;
};

enum class LegacyRenderPassKind {
    shadow,
    base_sprite,
    tint,
    collision_glow,
};

struct LegacyRenderPassSelection {
    bool shadow = true;
    bool main = true;
    bool global_shadows_enabled = true;
};

// Headless representation of the facts passed from the legacy sprite base to
// the renderer. Coordinates/pixel backend are deliberately excluded until the
// old world-transform and QuickDraw/terrain submission layers are separately
// reconstructed.
struct LegacyRenderIntent {
    LegacyRenderPassKind kind = LegacyRenderPassKind::base_sprite;
    FourCC sprite_face{};
    int sprite_frame = 0;
    int numeric_layer = 0;
    float scale = 0.0f;
    float visibility_percent = 0.0f;
    float effect_amount = 0.0f;
    Rgb24 effect_color{};
    bool draw_to_terrain = false;
    bool world_space = true;
};

[[nodiscard]] float legacy_percent_to_scale(int percent);

// 0x146F0 initial visual reset: initial unit visibility/scale are current
// values, state values become required targets/deltas, and initial scale
// tolerance consumes the original integer RNG helper when its half-range is
// non-zero. Frame selection remains owned by the animation runtime; callers
// may supply the already-selected frame.
[[nodiscard]] LegacySpriteVisualRuntime initialise_legacy_sprite_visual(
    const CompiledUnitBehavior& behavior,
    std::size_t state_index,
    LegacyRandom& random,
    int selected_frame = 0);

// Ordinary state changes replace face + visual targets but preserve current
// visibility/tint/scale so 0x12750/0x12840 can ramp from the existing values.
void apply_legacy_state_visual_targets(
    LegacySpriteVisualRuntime& runtime,
    const CompiledUnitStateBehavior& state,
    int selected_frame);

// Exact scalar movement semantics recovered from 0x12750/0x12840.
[[nodiscard]] LegacyVisualTickResult tick_legacy_visual_scalars(
    LegacySpriteVisualRuntime& runtime);

// 0x12940 geometry refresh. A `none` face clears half-extents but leaves the
// stale width/height words untouched, exactly as the PPC routine does. Normal
// sprite faces resolve dimensions through the 0x19CA0-style cache API and then
// halve each signed dimension with truncation toward zero. Returns false only
// when a non-none resource could not be resolved by the clean cache/loader.
[[nodiscard]] bool refresh_legacy_sprite_geometry(
    LegacySpriteVisualRuntime& runtime,
    LegacySpriteCache& cache,
    const LegacySpriteCache::Loader& loader = {});

// 0x12FA0 FourCC -> numeric render-layer switch. Unknown values retain the
// request template's zero/default layer code.
[[nodiscard]] int legacy_draw_layer_code(FourCC draw_layer, bool air_domain);
[[nodiscard]] int legacy_shadow_layer_code(FourCC draw_layer, bool air_domain);

struct LegacyShadowRuntimeConfig {
    float air_x_offset = -48.0f;
    float air_y_offset = 104.0f;
    float ground_x_offset = -6.0f;
    float ground_y_offset = 8.0f;
};

// Fixed Game[gafl] positions read by 0x13460 through 0x20250. The compiler
// validates labels so a shifted/modded table fails rather than silently
// changing shadow placement.
[[nodiscard]] std::optional<LegacyShadowRuntimeConfig> compile_legacy_shadow_runtime_config(
    const NamedTable<float>& game_floats,
    std::string* error = nullptr);

struct LegacyShadowTransformContext {
    // 0x100A0: bounded horizontal view offset (-32..31), applied only to
    // world-space non-terrain requests. Its higher-level controller remains
    // outside this pure transform.
    int horizontal_view_offset = 0;
    // 0xFEC0: current world/background Y origin, used by terrain submission.
    int world_y_origin = 0;
};

struct LegacyShadowRequestGeometry {
    int x = 0;
    int y = 0;
    int numeric_layer = 0;
    int transparency_0_to_32 = 32;
    float scale = 0.0f;
    bool draw_to_terrain = false;
};

// Exact position/scale/transparency portion of 0x13460. The caller supplies
// the sprite-base world position because that state belongs to the owning
// entity/player rather than LegacySpriteVisualRuntime itself.
[[nodiscard]] LegacyShadowRequestGeometry build_legacy_shadow_request_geometry(
    const LegacySpriteVisualRuntime& runtime,
    float world_x,
    float world_y,
    const LegacyShadowRuntimeConfig& config,
    LegacyShadowTransformContext context = {});

// 0x12F20/0x12FA0 request ordering at the clean renderer boundary. Entity
// castsShadows eligibility is applied here because the outer world renderer
// performs the legacy shadow-only/main-only choreography before 0x12F20.
[[nodiscard]] std::vector<LegacyRenderIntent> build_legacy_render_intents(
    const LegacySpriteVisualRuntime& runtime,
    LegacyRenderPassSelection selection = {});

} // namespace deimos
