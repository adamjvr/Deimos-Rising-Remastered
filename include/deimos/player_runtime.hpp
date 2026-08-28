#pragma once

#include "deimos/data_tables.hpp"
#include "deimos/entity_world.hpp"
#include "deimos/player_definition.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace deimos {

// Semantic subset of the compiled Player Definition consumed directly by the
// recovered 1.0.6 pickup/damage/lifecycle routines. The compiled Player
// Definition is NOT laid out in serialization order: notably
// entry_InvulnerabilityTime_INT lives at +0x8C, ahead of the entry-position
// block, while entry_InitialDelay_INT remains at +0xB8.
struct CompiledPlayerRuntimeDefinition {
    float default_shield_percentage = 100.0f;      // PlayerDef +0x48
    float shield_warning_percentage = 15.0f;      // +0x4C
    float shield_base_hit_percentage = 15.0f;     // +0x50
    int shield_hit_delay_ticks = 1;                // +0x54
    int life_max = 10;                             // +0x60
    int life_initial = 3;                          // +0x64
    FourCC life_spawn{};                           // +0x70

    int game_over_time_ticks = 20;                 // +0x80
    int dying_time_ticks = 80;                     // +0x84
    int final_dying_time_ticks = 40;               // +0x88
    int entry_invulnerability_time_ticks = 60;     // +0x8C
    int entry_solo_start_x = 208;                  // +0x90
    int entry_solo_start_y = 330;                  // +0x94
    int entry_multi_start_x = 104;                 // +0x98
    int entry_multi_start_y = 330;                 // +0x9C
    FourCC entry_spawn{};                           // +0xA0
    int entry_initial_delay_ticks = 55;            // +0xB8
    FourCC death_spawn{};                          // +0xBC
    FourCC active_spawn_on_hit{};                  // +0xC8
    FourCC active_shield_warning_object{};         // +0xCC
    FourCC active_defence_bonus_object{};          // +0xD0
};

enum class LegacyPlayerStatus : int {
    game_over = 1,
    waiting = 2,
    dying = 3,
    active = 4,
};

[[nodiscard]] CompiledPlayerRuntimeDefinition compile_player_runtime_definition(
    const PlayerDefinition& definition);

// Fixed Game[gafl] positions consumed by the collision/player-damage path.
// The original fetches these by index, so labels are verified at compile time
// to keep a shifted/modded table from silently changing semantics.
struct LegacyPlayerRuntimeGlobals {
    float impact_damage_to_entities = 100.0f; // Game[gafl] 161
    int delay_between_hit_spawns = 10;        // Game[gafl] 162, fctiwz
    int entity_hit_delay_ticks = 1;           // Game[gafl] 167, fctiwz
};

[[nodiscard]] std::optional<LegacyPlayerRuntimeGlobals> compile_legacy_player_runtime_globals(
    const NamedTable<float>& game_floats,
    std::string* error = nullptr);

// Fixed Objects[gaob] slots used by 0x27E50 when a dead player releases held
// money. Slots are descending denominations in the death routine.
struct LegacyPlayerRuntimeResources {
    FourCC money_50{}; // Objects[gaob] 2
    FourCC money_10{}; // Objects[gaob] 3
    FourCC money_5{};  // Objects[gaob] 4
    FourCC money_1{};  // Objects[gaob] 5
};

[[nodiscard]] std::optional<LegacyPlayerRuntimeResources> compile_legacy_player_runtime_resources(
    const NamedTable<FourCC>& game_objects,
    std::string* error = nullptr);

// Initialize only the gameplay fields reconstructed here. Geometry/identity
// remain owned by PlayerWorld and are intentionally left untouched.
void initialize_legacy_player_gameplay(
    PlayerRuntimeSlot& player,
    const CompiledPlayerRuntimeDefinition& definition);

struct LegacyPlayerPickupResult {
    bool accepted = true;
    FourCC pickup_type{};
    int pickup_value = 0;
    int money_before = 0;
    int money_after = 0;
    int lives_before = 0;
    int lives_after = 0;
    int multiplier_before = 1;
    int multiplier_after = 1;
    float shield_before = 0.0f;
    float shield_after = 0.0f;
    bool feedback_due = false;
    std::optional<FourCC> spawn_due;
};

// PPC 0x37580. Canonical 1.0.6 ships coin/mult/exli/shie pickups; the binary
// also retains air/grnd rejection branches and a no-op accepted spec/default
// branch. air/grnd are rejected while player +0xCE (invulnerability) is set.
[[nodiscard]] LegacyPlayerPickupResult apply_legacy_player_pickup(
    PlayerRuntimeSlot& player,
    const EntityRuntime& pickup,
    const CompiledPlayerRuntimeDefinition& definition);

struct LegacyPlayerMoneyDrop {
    int denomination = 0;
    int count = 0;
    FourCC spawn_id{};
};

struct LegacyPlayerDamageResult {
    bool processed = false;
    bool blocked_by_hit_delay = false;
    bool invulnerability_bypassed_shield_damage = false;
    float requested_damage = 0.0f;
    float scaled_shield_damage = 0.0f;
    float shield_before = 0.0f;
    float shield_after = 0.0f;
    bool hit_glow_due = false;
    std::optional<FourCC> spawn_on_hit_due;
    std::optional<FourCC> shield_warning_due;
    bool death_entered = false;
    std::optional<FourCC> death_spawn_due;
    int money_before_death = 0;
    std::vector<LegacyPlayerMoneyDrop> money_drops;
};

// PPC 0x27100 through its immediate death-entry helper 0x27E50. The caller
// supplies Game[gafl] 162 Player_DelayBetweenHitSpawns (canonical = 10).
// Death entry changes status to 3 and clears held money; life decrement is NOT
// performed by this routine and remains part of the later death/respawn state
// machine.
[[nodiscard]] LegacyPlayerDamageResult apply_legacy_player_damage(
    PlayerRuntimeSlot& player,
    const CompiledPlayerRuntimeDefinition& definition,
    float damage,
    std::uint32_t current_tick,
    int delay_between_hit_spawns = 10,
    const LegacyPlayerRuntimeResources* resources = nullptr);

// Headless reconstruction of PPC 0x2A150 plus the state/position subset of
// its respawn initializer 0x29CC0. The original fifth argument to 0x2A150 is
// a byte gate controlling whether an expired dying state consumes a life. Its
// direct caller feeds a global latch that becomes 1 once Player 1 first reaches
// active status 4, so the bounded higher-level meaning is "gameplay has
// started". `defer_invulnerability_expiry` models the separate 0x5CF0 gate
// consulted only while status 4 is invulnerable.
struct LegacyPlayerLifecycleResult {
    int status_before = 0;
    int status_after = 0;
    bool life_decremented = false;
    bool respawned = false;
    bool game_over_entered = false;
    bool disabled_after_game_over = false;
    bool invulnerability_cleared = false;
    bool active_entry_waiting = false;
    std::optional<EntityPoint> respawn_position;
    std::optional<FourCC> entry_spawn_due;
};

[[nodiscard]] LegacyPlayerLifecycleResult advance_legacy_player_lifecycle(
    PlayerRuntimeSlot& player,
    const CompiledPlayerRuntimeDefinition& definition,
    std::uint32_t current_tick,
    bool consume_life_on_death = true,
    bool defer_invulnerability_expiry = false);

} // namespace deimos
