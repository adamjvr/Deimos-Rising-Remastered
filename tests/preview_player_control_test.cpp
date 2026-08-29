#include "deimos/preview_player_control.hpp"

#include <cassert>
#include <cmath>
#include <string>

namespace {

bool near(float a, float b, float eps = 1.0e-5f) {
    return std::fabs(a - b) <= eps;
}

} // namespace

int main() {
    deimos::PlayerDefinition def;
    def.name = "Player 1";
    def.fields.add({"active_DefaultMaxSpeed_FLOAT", 7.8f, "7.8", 1});
    def.fields.add({"active_VelocityDelta_FLOAT", 1.6f, "1.6", 2});

    deimos::NamedTable<float> globals(184);
    for (std::size_t i = 0; i < globals.size(); ++i) {
        globals[i] = {"unused_" + std::to_string(i), 0.0f};
    }
    globals[183] = {"Player_TopGameAreaLimit", 13.0f};

    std::string error;
    const auto config = deimos::compile_preview_player_control_config(def, globals, 416, 480, &error);
    assert(config && error.empty());
    assert(near(config->max_speed, 7.8f));
    assert(near(config->velocity_delta, 1.6f));
    assert(near(config->top_game_area_limit, 13.0f));
    assert(near(config->max_x, 415.0f));
    assert(near(config->max_y, 479.0f));

    // Source-label guard prevents accidental table drift.
    auto shifted = globals;
    shifted[183].first = "Player_DefenceBonusBaseAmount";
    error.clear();
    assert(!deimos::compile_preview_player_control_config(def, shifted, 416, 480, &error));
    assert(!error.empty());

    deimos::PlayerRuntimeSlot player;
    player.status = 4;
    player.enabled = true;
    player.x = 208.0f;
    player.y = 330.0f;

    // Right input accelerates toward +7.8 in exact 1.6 increments.
    deimos::PreviewPlayerControlInput input;
    input.right = true;
    auto r = deimos::advance_preview_player_control(player, *config, input);
    assert(r.active);
    assert(near(r.target_velocity_x, 7.8f));
    assert(near(player.velocity_x, 1.6f));
    assert(near(player.x, 209.6f));
    for (int n = 0; n < 8; ++n) {
        (void)deimos::advance_preview_player_control(player, *config, input);
    }
    assert(near(player.velocity_x, 7.8f));

    // Releasing converges back toward zero rather than snapping.
    input.right = false;
    const float before_release = player.velocity_x;
    r = deimos::advance_preview_player_control(player, *config, input);
    assert(near(player.velocity_x, before_release - 1.6f));

    // Opposing inputs cancel to zero target.
    input.left = true;
    input.right = true;
    r = deimos::advance_preview_player_control(player, *config, input);
    assert(near(r.target_velocity_x, 0.0f));

    // Bounds clamp and kill only outward velocity on the clipped axis.
    player.x = 414.5f;
    player.velocity_x = 7.8f;
    input.left = false;
    input.right = true;
    r = deimos::advance_preview_player_control(player, *config, input);
    assert(r.clamped_x);
    assert(near(player.x, 415.0f));
    assert(near(player.velocity_x, 0.0f));

    player.y = 13.5f;
    player.velocity_y = -7.8f;
    input = {};
    input.up = true;
    r = deimos::advance_preview_player_control(player, *config, input);
    assert(r.clamped_y);
    assert(near(player.y, 13.0f));
    assert(near(player.velocity_y, 0.0f));

    // Inactive players are not moved by host control.
    player.status = 3;
    player.x = 100.0f;
    input = {};
    input.right = true;
    r = deimos::advance_preview_player_control(player, *config, input);
    assert(!r.active);
    assert(near(player.x, 100.0f));

    return 0;
}
