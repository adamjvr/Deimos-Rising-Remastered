#include "deimos/render_runtime.hpp"
#include "deimos/unit_behavior.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <string>

namespace {

deimos::FourCC id(const char (&text)[5]) {
    return deimos::FourCC{{text[0], text[1], text[2], text[3]}};
}

deimos::DefinitionField f_bool(const char* key, bool value) {
    return {key, value, value ? "TRUE" : "FALSE", 0};
}
deimos::DefinitionField f_int(const char* key, int value) {
    return {key, value, std::to_string(value), 0};
}
deimos::DefinitionField f_float(const char* key, float value) {
    return {key, value, std::to_string(value), 0};
}
deimos::DefinitionField f_string(const char* key, const char* value) {
    return {key, std::string(value), value, 0};
}
deimos::DefinitionField f_id(const char* key, deimos::FourCC value) {
    return {key, value, value.str(), 0};
}
deimos::DefinitionField f_color(const char* key, deimos::Rgb24 value) {
    return {key, value, "", 0};
}

void add_rule_state_defaults(deimos::UnitStateDefinition& state) {
    state.fields.add(f_float("stateOnRange_FLOAT", 0.0f));
    state.fields.add(f_string("stateOnRangeChangeTo_STR", ""));
    state.fields.add(f_string("stateOnHitChangeTo_STR", ""));
    state.fields.add(f_int("stateOnHitChangeStateDelay_INT", 0));
    state.fields.add(f_int("stateOnTimerMin_INT", 0));
    state.fields.add(f_int("stateOnTimerMax_INT", 0));
    state.fields.add(f_string("stateOnTimerChangeTo_STR", ""));
    state.fields.add(f_int("stateOnCounter_INT", 0));
    state.fields.add(f_string("stateOnCounterChangeTo_STR", ""));
}

deimos::UnitDefinition visual_unit() {
    deimos::UnitDefinition unit;
    unit.name = "Visual Test";
    unit.core_fields.add(f_bool("isGroundBased_BOOL", false));
    unit.core_fields.add(f_bool("castsShadows_BOOL", true));
    unit.core_fields.add(f_bool("adjustShadowLocForScaling_BOOL", true));
    unit.core_fields.add(f_int("initialScalePercent_INT", 80));
    unit.core_fields.add(f_int("initialScalePercentTolerance_INT", 20));
    unit.core_fields.add(f_int("initialVisibilityPercent_INT", 75));
    unit.core_fields.add(f_id("drawLayer_ID", id("defa")));

    deimos::UnitStateDefinition state;
    state.name = "Normal";
    add_rule_state_defaults(state);
    state.fields.add(f_id("stateSpriteFace_ID", id("ship")));
    state.fields.add(f_int("stateSpriteFrameMin_INT", 2));
    state.fields.add(f_int("stateSpriteFrameMax_INT", 7));
    state.fields.add(f_bool("stateUseParentDirection_BOOL", true));
    state.fields.add(f_int("stateRequiredVisibilityPercent_INT", 100));
    state.fields.add(f_int("stateVisibilityDeltaPercent_INT", 10));
    state.fields.add(f_int("stateRequiredScalePercent_INT", 120));
    state.fields.add(f_int("stateScaleDeltaPercent_INT", 5));
    state.fields.add(f_int("stateTintPercent_INT", 30));
    state.fields.add(f_int("stateTintDeltaPercent_INT", 4));
    state.fields.add(f_color("stateTintColor_COLOR", {10, 20, 30}));
    state.fields.add(f_bool("stateDoColorise_BOOL", false));
    state.fields.add(f_bool("stateDrawToTerrain_BOOL", true));
    unit.states.push_back(std::move(state));
    return unit;
}

bool near(float a, float b) {
    return std::fabs(a - b) < 0.00001f;
}

} // namespace

int main() {
    const auto unit = visual_unit();
    const auto behavior = deimos::compile_unit_behavior(unit);
    assert(behavior.initial_scale_percent == 80);
    assert(behavior.initial_scale_tolerance_percent == 20);
    assert(behavior.initial_visibility_percent == 75);
    assert(behavior.draw_layer == id("defa"));
    assert(behavior.casts_shadows);
    assert(behavior.adjust_shadow_location_for_scaling);

    const auto& state = behavior.states.at(0);
    assert(state.sprite_face == id("ship"));
    assert(state.sprite_frame_min == 2 && state.sprite_frame_max == 7);
    assert(state.use_parent_direction);
    assert(state.required_visibility_percent == 100);
    assert(state.visibility_delta_percent == 10);
    assert(state.required_scale_percent == 120);
    assert(state.scale_delta_percent == 5);
    assert(state.tint_percent == 30 && state.tint_delta_percent == 4);
    assert((state.tint_color == deimos::Rgb24{10, 20, 30}));
    assert(state.draw_to_terrain);

    // Initial scale tolerance is half the serialized tolerance on each side,
    // then mapped through the original inclusive integer RNG helper.
    deimos::LegacyRandom expected_rng(0x12345678u);
    const int jitter = deimos::choose_inclusive_integer(-10, 10, expected_rng);
    deimos::LegacyRandom runtime_rng(0x12345678u);
    auto runtime = deimos::initialise_legacy_sprite_visual(
        behavior, 0, runtime_rng, 4);
    assert(runtime_rng.seed() == expected_rng.seed());
    assert(runtime.sprite_face == id("ship") && runtime.sprite_frame == 4);
    assert(runtime.air_domain);
    assert(runtime.world_space);
    assert(runtime.casts_shadows);
    assert(runtime.adjust_shadow_location_for_scaling);
    assert(runtime.draw_to_terrain);
    assert(near(runtime.visibility_percent, 75.0f));
    assert(near(runtime.required_visibility_percent, 100.0f));
    assert(near(runtime.tint_percent, 30.0f));
    assert(near(runtime.required_tint_percent, 30.0f));
    assert(near(runtime.scale, static_cast<float>(80 + jitter) / 100.0f));
    assert(near(runtime.required_scale, 1.20f));
    assert(near(runtime.scale_delta, 0.05f));

    // 0x12750 and 0x12840 move current values toward targets with overshoot
    // clamps; visibility/tint additionally clamp their decreasing side at 0.
    runtime.visibility_percent = 95.0f;
    runtime.required_visibility_percent = 100.0f;
    runtime.visibility_delta_percent = 10.0f;
    runtime.tint_percent = 3.0f;
    runtime.required_tint_percent = 0.0f;
    runtime.tint_delta_percent = 4.0f;
    runtime.scale = 1.18f;
    runtime.required_scale = 1.20f;
    runtime.scale_delta = 0.05f;
    runtime.bounds_dirty = false;
    const auto tick = deimos::tick_legacy_visual_scalars(runtime);
    assert(tick.visibility_changed && near(runtime.visibility_percent, 100.0f));
    assert(tick.tint_changed && near(runtime.tint_percent, 0.0f));
    assert(tick.scale_changed && near(runtime.scale, 1.20f));
    assert(runtime.bounds_dirty);

    // Ordinary state target updates preserve current scalar values. A target
    // scale change alone does not dirty geometry until 0x12840 actually moves
    // the current scale; a face/frame change does.
    auto next_state = state;
    next_state.required_visibility_percent = 40;
    next_state.visibility_delta_percent = 3;
    next_state.required_scale_percent = 50;
    next_state.scale_delta_percent = 2;
    next_state.tint_percent = 60;
    next_state.tint_delta_percent = 5;
    runtime.visibility_percent = 91.0f;
    runtime.tint_percent = 12.0f;
    runtime.scale = 1.10f;
    runtime.sprite_frame = 4;
    runtime.bounds_dirty = false;
    deimos::apply_legacy_state_visual_targets(runtime, next_state, 4);
    assert(near(runtime.visibility_percent, 91.0f));
    assert(near(runtime.tint_percent, 12.0f));
    assert(near(runtime.scale, 1.10f));
    assert(near(runtime.required_scale, 0.50f));
    assert(!runtime.bounds_dirty);
    next_state.sprite_face = id("next");
    deimos::apply_legacy_state_visual_targets(runtime, next_state, 4);
    assert(runtime.bounds_dirty);

    // Scale has no visibility/tint-style zero clamp in 0x12840.
    runtime.scale = 0.02f;
    runtime.required_scale = -0.10f;
    runtime.scale_delta = 0.05f;
    runtime.bounds_dirty = false;
    const auto negative_scale_tick = deimos::tick_legacy_visual_scalars(runtime);
    assert(negative_scale_tick.scale_changed);
    assert(near(runtime.scale, -0.03f));
    assert(runtime.bounds_dirty);

    // Draw-layer switch recovered from 0x12FA0.
    assert(deimos::legacy_draw_layer_code(id("defa"), false) == 3);
    assert(deimos::legacy_draw_layer_code(id("defa"), true) == 7);
    assert(deimos::legacy_draw_layer_code(id("grou"), false) == 3);
    assert(deimos::legacy_draw_layer_code(id("grhi"), false) == 5);
    assert(deimos::legacy_draw_layer_code(id("ailo"), false) == 7);
    assert(deimos::legacy_draw_layer_code(id("aihi"), false) == 8);
    assert(deimos::legacy_draw_layer_code(id("plwe"), false) == 9);
    assert(deimos::legacy_draw_layer_code(id("play"), false) == 10);
    assert(deimos::legacy_draw_layer_code(id("plsh"), false) == 11);
    assert(deimos::legacy_draw_layer_code(id("plef"), false) == 12);
    assert(deimos::legacy_draw_layer_code(id("plui"), false) == 13);
    assert(deimos::legacy_draw_layer_code(id("atmo"), false) == 14);
    assert(deimos::legacy_draw_layer_code(id("hud "), false) == 15);
    assert(deimos::legacy_draw_layer_code(id("none"), false) == 3);
    assert(deimos::legacy_draw_layer_code(id("none"), true) == 7);
    assert(deimos::legacy_draw_layer_code(id("????"), false) == 0);
    assert(deimos::legacy_shadow_layer_code(id("defa"), false) == 2);
    assert(deimos::legacy_shadow_layer_code(id("defa"), true) == 6);
    assert(deimos::legacy_shadow_layer_code(id("grou"), false) == 2);
    assert(deimos::legacy_shadow_layer_code(id("grhi"), false) == 4);
    assert(deimos::legacy_shadow_layer_code(id("plwe"), false) == 6);

    // 0x10C20 and tint-pass request-domain conversions. Visibility is
    // integer-truncated before mapping into 0..32 destination weight.
    assert(deimos::legacy_percent_to_transparency_0_to_32(100.0f) == 0);
    assert(deimos::legacy_percent_to_transparency_0_to_32(80.9f) == 6);
    assert(deimos::legacy_percent_to_transparency_0_to_32(0.0f) == 32);
    assert(deimos::legacy_percent_to_transparency_0_to_32(150.0f) == 16);
    assert(deimos::legacy_percent_to_transparency_0_to_32(-10.0f) == 32);
    assert(deimos::legacy_tint_effect_transparency_0_to_32(25.0f, 100.0f) == 24);
    assert(deimos::legacy_tint_effect_transparency_0_to_32(25.0f, 80.0f) == 25);

    // Shared PPC 0x12BC0/0x12C10 collision/pickup glow pulse. Amount 32 is
    // the no-effect endpoint, 4 is the peak, and rate 6 gives the shipped
    // 32->26->20->14->8->4->10->16->22->28->32 sequence.
    deimos::LegacySpriteVisualRuntime glow;
    deimos::trigger_legacy_collision_glow(glow, {255, 255, 255});
    assert(glow.collision_glow_active && glow.collision_glow_amount_0_to_32 == 32);
    const int expected_glow[] = {26,20,14,8,4,10,16,22,28,32};
    for (int expected : expected_glow) {
        deimos::tick_legacy_collision_glow(glow);
        assert(glow.collision_glow_amount_0_to_32 == expected);
    }
    assert(!glow.collision_glow_active);
    deimos::trigger_legacy_collision_glow(glow, {255,255,255});
    deimos::tick_legacy_collision_glow(glow);
    deimos::trigger_legacy_collision_glow(glow, {1,2,3}, 6, false);
    assert(glow.collision_glow_amount_0_to_32 == 26);
    assert(glow.collision_glow_color.red == 255);

    // Wrapper order is shadow before main. Normal base, tint, and glow are
    // independently generated; stateDoColorise suppresses only the base pass.
    runtime.visibility_percent = 80.0f;
    runtime.tint_percent = 25.0f;
    runtime.tint_color = {1, 2, 3};
    runtime.collision_glow_active = true;
    runtime.collision_glow_amount_0_to_32 = 17;
    runtime.collision_glow_color = {4, 5, 6};
    runtime.do_colorise = false;
    auto intents = deimos::build_legacy_render_intents(runtime);
    assert(intents.size() == 4);
    assert(intents[0].kind == deimos::LegacyRenderPassKind::shadow);
    assert(intents[1].kind == deimos::LegacyRenderPassKind::base_sprite);
    assert(intents[2].kind == deimos::LegacyRenderPassKind::tint);
    assert(intents[3].kind == deimos::LegacyRenderPassKind::collision_glow);
    assert(intents[0].effect_amount_0_to_32 == 20); // shadow min-transparency clamp
    assert(intents[1].effect_amount_0_to_32 == 6);  // 80% visibility
    assert(intents[2].effect_amount_0_to_32 == 25); // 25% tint at 80% visibility
    assert(intents[3].effect_amount_0_to_32 == 17); // collision glow is already raw 0..32
    assert(intents[0].numeric_layer == 0); // 0x13460 shadow terrain submission
    for (std::size_t i = 1; i < intents.size(); ++i) {
        assert(intents[i].numeric_layer == 1); // 0x12FA0 main terrain submission
    }
    for (const auto& intent : intents) assert(intent.draw_to_terrain);


    runtime.draw_to_terrain = false;
    auto layered = deimos::build_legacy_render_intents(runtime);
    assert(layered[0].numeric_layer == 6); // air shadow
    assert(layered[1].numeric_layer == 7); // air default main
    runtime.draw_to_terrain = true;

    // +0x90 terrain submission gate is strict and updates only on success.
    runtime.last_terrain_submit_sequence = 0;
    assert(!deimos::legacy_terrain_submission_due(runtime, 0));
    assert(deimos::legacy_terrain_submission_due(runtime, 1));
    assert(runtime.last_terrain_submit_sequence == 1);
    assert(!deimos::legacy_terrain_submission_due(runtime, 1));
    assert(!deimos::legacy_terrain_submission_due(runtime, 0));
    assert(deimos::legacy_terrain_submission_due(runtime, 2));
    runtime.draw_to_terrain = false;
    assert(!deimos::legacy_terrain_submission_due(runtime, 3));
    runtime.draw_to_terrain = true;

    runtime.do_colorise = true;
    intents = deimos::build_legacy_render_intents(runtime);
    assert(intents.size() == 3);
    assert(intents[0].kind == deimos::LegacyRenderPassKind::shadow);
    assert(intents[1].kind == deimos::LegacyRenderPassKind::tint);
    assert(intents[2].kind == deimos::LegacyRenderPassKind::collision_glow);

    // Outer renderer selection can request a shadow-only or main-only pass,
    // and the user/global shadow gate suppresses the shadow request entirely.
    intents = deimos::build_legacy_render_intents(runtime, {true, false, true});
    assert(intents.size() == 1 && intents[0].kind == deimos::LegacyRenderPassKind::shadow);
    intents = deimos::build_legacy_render_intents(runtime, {true, false, false});
    assert(intents.empty());

    runtime.visibility_percent = 0.0f;
    assert(deimos::build_legacy_render_intents(runtime).empty());
    runtime.visibility_percent = 80.0f;
    runtime.sprite_face = id("none");
    assert(deimos::build_legacy_render_intents(runtime).empty());

    // HUD entities clear sprite-base +0x18 and therefore do not take the
    // world-space coordinate transform path.
    auto hud_behavior = behavior;
    hud_behavior.draw_layer = id("hud ");
    deimos::LegacyRandom hud_rng(1);
    const auto hud_runtime = deimos::initialise_legacy_sprite_visual(hud_behavior, 0, hud_rng, 2);
    assert(!hud_runtime.world_space);

    std::cout << "render runtime tests passed\n";
    // 0x12940 sprite-resource geometry refresh.
    deimos::LegacySpriteCache sprite_cache;
    assert(sprite_cache.publish({id("vis1"), {
        {deimos::LegacySpriteRect{0,0,53,43}, 53, 43},
        {deimos::LegacySpriteRect{0,0,20,10}, 20, 10},
    }}));
    deimos::LegacySpriteVisualRuntime geometry;
    geometry.sprite_face = id("vis1");
    geometry.sprite_frame = 0;
    geometry.scale = 1.5f;
    geometry.bounds_dirty = true;
    assert(deimos::refresh_legacy_sprite_geometry(geometry, sprite_cache));
    assert(geometry.sprite_width == 79 && geometry.sprite_height == 64);
    assert(geometry.half_width == 39 && geometry.half_height == 32);
    assert(!geometry.bounds_dirty);

    // `none` only zeros half extents; 0x12940 leaves width/height stale.
    geometry.sprite_face = id("none");
    geometry.sprite_width = 123;
    geometry.sprite_height = 456;
    geometry.half_width = 61;
    geometry.half_height = 228;
    geometry.bounds_dirty = true;
    assert(deimos::refresh_legacy_sprite_geometry(geometry, sprite_cache));
    assert(geometry.sprite_width == 123 && geometry.sprite_height == 456);
    assert(geometry.half_width == 0 && geometry.half_height == 0);

    return 0;
}
