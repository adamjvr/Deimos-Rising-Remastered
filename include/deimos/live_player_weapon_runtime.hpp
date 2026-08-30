#pragma once

#include "deimos/game_definitions.hpp"
#include "deimos/entity_world.hpp"
#include "deimos/spawn_runtime.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace deimos {

// Modern host-control bridge for the already-typed original Weapon
// Definitions. This intentionally does not assign unresolved film/InputSprocket
// bit meanings; it only turns explicit host actions into deterministic weapon
// launch requests using canonical serialized timing/offset fields.
struct LivePlayerWeaponInput {
    bool fire_air = false;
    bool fire_ground = false;
    bool switch_air = false;
};

struct LivePlayerWeaponSlot {
    FourCC id{};
    std::string name;
    FourCC type{};
    FourCC default_marker{};
    int minimum_level_available = 0;
    int maximum_level_available = 9999;
    bool auto_repeat = false;
    int delay_between_launches = 0;

    // Serialized Weapon Definition power-up contract. Air weapons in the
    // canonical corpus use these fields for hold-to-charge behavior and the
    // score-bar power meter. Ground Plasma Bomb leaves them zero/none.
    int powerup_air_time_until_activation = 0;
    FourCC powerup_air_activation_spawn_id{};
    int powerup_air_time_between_power_level_changes = 0;
    int powerup_air_max_power_level = 0;
    int powerup_air_overload_time = 0;
    FourCC powerup_air_release_spawn_id{};
    int powerup_air_time_between_release_spawns = 0;
    bool powerup_air_do_release_on_max_power_level = false;

    FourCC player1_appearance_face{};
    FourCC player2_appearance_face{};
    FourCC score_bar_preview_face{};
    int score_bar_preview_frame = 0;
    std::vector<WeaponSpawn> spawns;
};

struct LivePlayerWeaponCatalog {
    std::vector<LivePlayerWeaponSlot> air;
    std::vector<LivePlayerWeaponSlot> ground;
    std::size_t default_air = 0;
    std::size_t default_ground = 0;
};

struct LivePlayerWeaponState {
    std::size_t selected_air = 0;
    std::size_t selected_ground = 0;
    std::optional<std::uint32_t> last_air_launch_tick;
    std::optional<std::uint32_t> last_ground_launch_tick;
    bool air_was_down = false;
    bool ground_was_down = false;
    bool switch_was_down = false;

    // Host-side reconstruction of the serialized hold-to-charge weapon
    // fields. These are deliberately explicit so the not-yet-closed PPC
    // caller can later replace the orchestration without changing the data
    // contract or HUD consumers.
    std::optional<std::uint32_t> air_hold_started_tick;
    std::optional<std::uint32_t> air_powerup_activated_tick;
    int air_power_level = 0;
    int pending_air_release_spawns = 0;
    std::optional<std::uint32_t> next_air_release_spawn_tick;
    FourCC pending_air_release_spawn_id{};
};

struct LivePlayerWeaponLaunch {
    FourCC weapon_id{};
    bool ground_weapon = false;
    std::vector<SpawnRequestSeed> requests;
};

struct LivePlayerWeaponStepResult {
    bool air_launched = false;
    bool ground_launched = false;
    bool air_switched = false;
    bool air_powerup_activated = false;
    bool air_powerup_released = false;
    bool air_powerup_overloaded = false;
    int air_power_level = 0;
    float air_power_percentage = 0.0f;
    std::optional<LivePlayerWeaponLaunch> air_launch;
    std::optional<LivePlayerWeaponLaunch> ground_launch;

    // Activation and charged-release units are normal canonical Unit
    // Definitions, but are not part of the weapon's ordinary spawn list.
    // Surface them separately so the world host can construct them without
    // hiding timing or ownership semantics inside this control bridge.
    std::vector<SpawnRequestSeed> powerup_requests;
};

[[nodiscard]] LivePlayerWeaponCatalog compile_live_player_weapon_catalog(
    const GameDefinitions& definitions);

void initialize_live_player_weapon_state(
    LivePlayerWeaponState& state,
    const LivePlayerWeaponCatalog& catalog,
    int level_number);

[[nodiscard]] const LivePlayerWeaponSlot* selected_live_air_weapon(
    const LivePlayerWeaponCatalog& catalog,
    const LivePlayerWeaponState& state) noexcept;

[[nodiscard]] const LivePlayerWeaponSlot* selected_live_ground_weapon(
    const LivePlayerWeaponCatalog& catalog,
    const LivePlayerWeaponState& state) noexcept;

// One host-action tick. Player X/Y are in the same visible-game coordinate
// domain used by the clean player runtime. Weapon spawn offsets are applied in
// serialized order and produce normal 44-byte-constructor semantic seeds.
[[nodiscard]] LivePlayerWeaponStepResult advance_live_player_weapons(
    const LivePlayerWeaponCatalog& catalog,
    LivePlayerWeaponState& state,
    const LivePlayerWeaponInput& input,
    const PlayerRuntimeSlot& player,
    std::uint32_t current_tick,
    int level_number);

} // namespace deimos
