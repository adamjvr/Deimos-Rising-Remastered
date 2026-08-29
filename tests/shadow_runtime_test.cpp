#include "deimos/render_runtime.hpp"

#include <cassert>
#include <cmath>
#include <string>

namespace {
deimos::FourCC id(const char* s) { return deimos::FourCC{{s[0],s[1],s[2],s[3]}}; }
bool near(float a, float b) { return std::fabs(a - b) < 0.00001f; }
}

int main() {
    using namespace deimos;

    NamedTable<float> floats(52);
    for (std::size_t i = 0; i < floats.size(); ++i) floats[i] = {"unused", 0.0f};
    floats[48] = {"Shadow_XOffset", -48.0f};
    floats[49] = {"Shadow_YOffset", 104.0f};
    floats[50] = {"Shadow_GroundXOffset", -6.0f};
    floats[51] = {"Shadow_GroundYOffset", 8.0f};
    std::string error;
    auto config = compile_legacy_shadow_runtime_config(floats, &error);
    assert(config && error.empty());

    LegacySpriteVisualRuntime runtime;
    runtime.draw_layer = id("defa");
    runtime.air_domain = true;
    runtime.world_space = true;
    runtime.scale = 1.5f;
    runtime.visibility_percent = 100.0f;

    // Air default: shadow scale is 0.5*entity scale, but with the adjustment
    // flag off the X/Y offsets retain the fixed 0.5 basis (-24,+52).
    auto shadow = build_legacy_shadow_request_geometry(
        runtime, 100.75f, 200.25f, *config, {7, 300});
    assert(shadow.numeric_layer == 6);
    assert(near(shadow.scale, 0.75f));
    assert(shadow.transparency_0_to_32 == 20); // minimum legacy transparency
    assert(shadow.x == 69);  // trunc(100.75 - 24 - 7)
    assert(shadow.y == 252); // trunc(200.25 + 52)

    // With adjustShadowLocForScaling, the same air offsets use 0.5*entity
    // scale, so -48*0.75=-36 and 104*0.75=78.
    runtime.adjust_shadow_location_for_scaling = true;
    shadow = build_legacy_shadow_request_geometry(runtime, 100.75f, 200.25f, *config, {7, 300});
    assert(shadow.x == 57);
    assert(shadow.y == 278);

    // Explicit ground layer overrides an air-domain entity for shadow offset
    // and scale selection exactly as the 0x13460 layer switch does.
    runtime.draw_layer = id("grhi");
    runtime.adjust_shadow_location_for_scaling = true;
    runtime.visibility_percent = 50.9f; // fctiwz -> 50 -> transparency 16 -> min 20
    shadow = build_legacy_shadow_request_geometry(runtime, 100.75f, 200.25f, *config, {7, 300});
    assert(shadow.numeric_layer == 4);
    assert(near(shadow.scale, 1.5f));
    assert(shadow.x == 84);  // -6*1.5=-9, then -7 view shift
    assert(shadow.y == 212); // +8*1.5=+12
    assert(shadow.transparency_0_to_32 == 20);

    // Low visibility becomes more transparent; 25 -> abs(8-32)=24.
    runtime.visibility_percent = 25.0f;
    shadow = build_legacy_shadow_request_geometry(runtime, 0.0f, 0.0f, *config);
    assert(shadow.transparency_0_to_32 == 24);
    runtime.visibility_percent = 0.1f; // fctiwz -> 0 -> 32
    shadow = build_legacy_shadow_request_geometry(runtime, 0.0f, 0.0f, *config);
    assert(shadow.transparency_0_to_32 == 32);

    // Terrain submission bypasses normal shadow layers, ignores 0x100A0,
    // subtracts 32 from X, and adds 0xFEC0 to Y.
    runtime.draw_layer = id("grou");
    runtime.draw_to_terrain = true;
    runtime.world_space = true;
    runtime.scale = 1.0f;
    runtime.visibility_percent = 100.0f;
    shadow = build_legacy_shadow_request_geometry(runtime, 100.75f, 200.25f, *config, {31, 300});
    assert(shadow.numeric_layer == 0);
    assert(shadow.draw_to_terrain);
    assert(shadow.x == 62);  // trunc(100.75 - 6 - 32)
    assert(shadow.y == 508); // trunc(200.25 + 8 + 300)

    // HUD/non-world-space paths do not apply the bounded horizontal view
    // offset, but explicit HUD layer still uses air shadow offsets/layer 6.
    runtime.draw_to_terrain = false;
    runtime.draw_layer = id("hud ");
    runtime.world_space = false;
    runtime.scale = 1.0f;
    runtime.adjust_shadow_location_for_scaling = false;
    shadow = build_legacy_shadow_request_geometry(runtime, 100.0f, 50.0f, *config, {31, 0});
    assert(shadow.numeric_layer == 6);
    assert(shadow.x == 76);
    assert(shadow.y == 102);

    return 0;
}
