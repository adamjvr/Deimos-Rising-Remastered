#include "deimos/preview_player_control.hpp"

#include <algorithm>
#include <cmath>
#include <string_view>

namespace deimos {
namespace {

void fail(std::string* error, std::string message) {
    if (error) *error = std::move(message);
}

float approach(float current, float target, float delta) {
    if (current < target) return std::min(current + delta, target);
    if (current > target) return std::max(current - delta, target);
    return current;
}

float axis_target(bool negative, bool positive, float max_speed) {
    if (negative == positive) return 0.0f;
    return negative ? -max_speed : max_speed;
}

} // namespace

std::optional<PreviewPlayerControlConfig> compile_preview_player_control_config(
    const PlayerDefinition& player_definition,
    const NamedTable<float>& game_floats,
    int visible_game_width,
    int visible_game_height,
    std::string* error) {
    const auto max_speed = player_definition.fields.float_value("active_DefaultMaxSpeed_FLOAT");
    const auto velocity_delta = player_definition.fields.float_value("active_VelocityDelta_FLOAT");
    if (!max_speed || !velocity_delta) {
        fail(error, "Player Definition is missing active movement tuning fields");
        return std::nullopt;
    }
    if (!std::isfinite(*max_speed) || !std::isfinite(*velocity_delta) ||
        *max_speed < 0.0f || *velocity_delta < 0.0f) {
        fail(error, "Player Definition contains invalid active movement tuning");
        return std::nullopt;
    }
    constexpr std::size_t top_limit_index = 183;
    constexpr std::string_view top_limit_label = "Player_TopGameAreaLimit";
    if (game_floats.size() <= top_limit_index ||
        game_floats[top_limit_index].first != top_limit_label) {
        fail(error, "Game[gafl] 183 is not Player_TopGameAreaLimit");
        return std::nullopt;
    }
    if (visible_game_width <= 0 || visible_game_height <= 0) {
        fail(error, "visible gameplay dimensions must be positive");
        return std::nullopt;
    }

    PreviewPlayerControlConfig out;
    out.max_speed = *max_speed;
    out.velocity_delta = *velocity_delta;
    out.top_game_area_limit = game_floats[top_limit_index].second;
    out.min_x = 0.0f;
    out.max_x = static_cast<float>(visible_game_width - 1);
    out.max_y = static_cast<float>(visible_game_height - 1);
    if (!std::isfinite(out.top_game_area_limit) ||
        out.top_game_area_limit < 0.0f || out.top_game_area_limit > out.max_y) {
        fail(error, "Player_TopGameAreaLimit lies outside the visible gameplay area");
        return std::nullopt;
    }
    return out;
}

PreviewPlayerControlResult advance_preview_player_control(
    PlayerRuntimeSlot& player,
    const PreviewPlayerControlConfig& config,
    const PreviewPlayerControlInput& input) {
    PreviewPlayerControlResult out;
    out.x_before = player.x;
    out.y_before = player.y;
    out.velocity_x_before = player.velocity_x;
    out.velocity_y_before = player.velocity_y;

    if (!player.enabled || player.status != 4) {
        out.x_after = player.x;
        out.y_after = player.y;
        out.velocity_x_after = player.velocity_x;
        out.velocity_y_after = player.velocity_y;
        return out;
    }
    out.active = true;

    out.target_velocity_x = axis_target(input.left, input.right, config.max_speed);
    out.target_velocity_y = axis_target(input.up, input.down, config.max_speed);
    player.velocity_x = approach(player.velocity_x, out.target_velocity_x, config.velocity_delta);
    player.velocity_y = approach(player.velocity_y, out.target_velocity_y, config.velocity_delta);

    player.x += player.velocity_x;
    player.y += player.velocity_y;

    const float clamped_x = std::clamp(player.x, config.min_x, config.max_x);
    if (clamped_x != player.x) {
        out.clamped_x = true;
        player.x = clamped_x;
        if ((player.x <= config.min_x && player.velocity_x < 0.0f) ||
            (player.x >= config.max_x && player.velocity_x > 0.0f)) {
            player.velocity_x = 0.0f;
        }
    }
    const float clamped_y = std::clamp(player.y, config.top_game_area_limit, config.max_y);
    if (clamped_y != player.y) {
        out.clamped_y = true;
        player.y = clamped_y;
        if ((player.y <= config.top_game_area_limit && player.velocity_y < 0.0f) ||
            (player.y >= config.max_y && player.velocity_y > 0.0f)) {
            player.velocity_y = 0.0f;
        }
    }

    out.x_after = player.x;
    out.y_after = player.y;
    out.velocity_x_after = player.velocity_x;
    out.velocity_y_after = player.velocity_y;
    return out;
}

} // namespace deimos
