#pragma once

#include "deimos/data_tables.hpp"
#include "deimos/entity_world.hpp"
#include "deimos/player_definition.hpp"

#include <optional>
#include <string>

namespace deimos {

// Host-control integration used by the original-data live preview while the
// exact original InputSprocket/film-bit player-control dispatcher is still
// being recovered. The tuning values themselves come from canonical 1.0.6
// data and are source-label verified; the modern key-to-direction mapping is
// intentionally kept outside replay semantics.
struct PreviewPlayerControlConfig {
    float max_speed = 7.8f;          // active_DefaultMaxSpeed_FLOAT
    float velocity_delta = 1.6f;     // active_VelocityDelta_FLOAT
    float top_game_area_limit = 13.0f; // Game[gafl] 183 Player_TopGameAreaLimit
    float min_x = 0.0f;
    float max_x = 415.0f;
    float max_y = 479.0f;
};

struct PreviewPlayerControlInput {
    bool left = false;
    bool right = false;
    bool up = false;
    bool down = false;
};

struct PreviewPlayerControlResult {
    bool active = false;
    float target_velocity_x = 0.0f;
    float target_velocity_y = 0.0f;
    float x_before = 0.0f;
    float y_before = 0.0f;
    float x_after = 0.0f;
    float y_after = 0.0f;
    float velocity_x_before = 0.0f;
    float velocity_y_before = 0.0f;
    float velocity_x_after = 0.0f;
    float velocity_y_after = 0.0f;
    bool clamped_x = false;
    bool clamped_y = false;
};

[[nodiscard]] std::optional<PreviewPlayerControlConfig> compile_preview_player_control_config(
    const PlayerDefinition& player_definition,
    const NamedTable<float>& game_floats,
    int visible_game_width,
    int visible_game_height,
    std::string* error = nullptr);

// Deterministic inertial host-control bridge for the live integration fixture.
// Opposing directions cancel; otherwise velocity converges component-wise to
// +/- max_speed by velocity_delta, then position integrates and is constrained
// to the visible gameplay rectangle. This routine is deliberately named
// "preview" until the original player input dispatcher is instruction-closed.
[[nodiscard]] PreviewPlayerControlResult advance_preview_player_control(
    PlayerRuntimeSlot& player,
    const PreviewPlayerControlConfig& config,
    const PreviewPlayerControlInput& input);

} // namespace deimos
