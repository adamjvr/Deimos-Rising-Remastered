#pragma once

#include "deimos/data_tables.hpp"
#include "deimos/entity_world.hpp"
#include "deimos/particle_runtime.hpp"
#include "deimos/terrain_runtime.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace deimos {

// Data-driven mirror of Game.gafl[209..219] + Game.gaob[25..34], consumed by
// PPC 0x16528..0x167B8. Thresholds are already fctiwz-truncated integers.
struct LegacyRandomBonusConfig {
    std::array<int, 9> percent_thresholds{}; // bonus 1..9 upper bounds
    int ground_accuracy_reward_percent = 0; // Game.gafl[218]
    int minimum_progression_for_highest_bonus = 0; // Game.gafl[219]
    std::array<FourCC, 10> bonus_ids{};      // Game.gaob[25..34]
};

struct LegacyRandomBonusContext {
    // PPC 0x5CD0 game-context +0x14. The same unresolved progression value is
    // used by level-scaled shield construction.
    int progression_value = 1;

    // PPC 0x5D00 / 0x5D10 game-context +0x0C. Game.gafl names the special
    // branch GroundAccuracyReward, so this conservative name is evidence-led.
    bool ground_accuracy_reward_pending = false;
};

struct LegacyRandomBonusSelection {
    int roll = -1;
    FourCC unit_id{};
    bool special_ground_accuracy_reward = false;
    bool consumed_ground_accuracy_reward = false;
};

// Binds the positional resources consumed by the 1.0.6 executable:
// Game[gafl] indices 209..219 and Objects[gaob] indices 25..34. The canonical
// labels are verified as an additional guard against accidentally supplying a
// different float/ID list with the same length. Float values are converted with
// truncation toward zero, matching PPC fctiwz at the runtime comparisons.
[[nodiscard]] std::optional<LegacyRandomBonusConfig> compile_legacy_random_bonus_config(
    const NamedTable<float>& game_floats,
    const NamedTable<FourCC>& game_objects,
    std::string* error = nullptr);

// Pure selector for the PPC 0x16538..0x167B8 threshold chain. `roll` is the
// inclusive 0..100 result already drawn by 0x46580.
[[nodiscard]] LegacyRandomBonusSelection select_legacy_random_bonus(
    int roll,
    LegacyRandomBonusContext& context,
    const LegacyRandomBonusConfig& config);

enum class LegacyRemovalConsequenceKind {
    child_destroy,
    child_delete,
    terrain_draw,
    destruction_particles,
    destruction_spawn,
    water_impact_spawn,
    destruction_notice,
    destruction_sound,
    ordinary_coin_spawn,
    group_kill_coin_spawn,
    random_bonus_spawn,
    obstacle_create,
    deletion_spawn,
    owner_destruction_triggered,
    member_removed
};

struct LegacyRemovalConsequence {
    LegacyRemovalConsequenceKind kind = LegacyRemovalConsequenceKind::member_removed;
    EntityHandle source = kNoEntityHandle;
    EntityHandle related = kNoEntityHandle;
    FourCC resource_id{};
    std::string text;
    Rgb24 color{};
    bool ground_based = false;
    std::uint32_t tick = 0;
    CompiledDestructionSoundBehavior sound{};
    std::optional<SpawnRequestSeed> spawn_request;
    std::optional<LegacyParticleSpawnRequest> particle_spawn;
    bool particle_executed = false;
    std::optional<RectI> rectangle;
    bool casts_shadows = false;
};

struct LegacyMemberRemovalRecord {
    EntityHandle member = kNoEntityHandle;
    std::uint32_t group_serial = 0;
    bool destruction = false;
    bool player_attributed = false;
    bool group_killed = false;
    bool group_should_be_removed = false;
};

struct LegacyRemovalTrace {
    std::vector<LegacyRemovalConsequence> consequences;
    std::vector<LegacyMemberRemovalRecord> removals;
};

struct LegacyRemovalContext {
    std::uint32_t current_tick = 0;
    LegacyRandomBonusContext random_bonus{};
    LegacyRandomBonusConfig random_bonus_config{};

    // PPC 0x16880 terrain/media path. The canonical Objects[gaob] binding is
    // fixed at slots 6..9; a loaded level supplies the Media Mask probe and the
    // current vertical background origin returned by 0xFEC0.
    int world_y_origin = 0;
    LegacyWaterImpactConfig water_impact_config{};
    LegacyWaterMaskProbe water_probe;

    // PPC 0x2A6D0 persistent rectangle list used by destructDrawToTerrain and
    // queried by collidesWithGroundObstacles. Null keeps isolated tests/headless
    // callers from manufacturing terrain state they do not own.
    LegacyGroundObstacleRects* ground_obstacles = nullptr;

    // Optional exact execution bridge for the inline destruction-particle call
    // to PPC 0x43340. Consequence facts are still emitted when this is absent.
    LegacyParticleExecutionContext particle_execution{};
};

// PPC 0x16300. Emits deterministic headless consequence facts in original
// order and marks the entity destroyed. Calling it twice is idempotent, just as
// the original +0xCB guard suppresses a second destruction-effects pass.
[[nodiscard]] bool apply_legacy_destruction_effects(
    EntityRuntime& entity,
    std::int8_t source_owner_index,
    LegacyRemovalContext& context,
    LegacyRandom& random,
    LegacyRemovalTrace& trace);

// PPC 0x36120. Performs child cascades, destroyed-member/group-kill accounting,
// player-attributed coin rewards, destruction effects, active-member decrement,
// and the special SERM group-removal exemption. The clean world keeps removed
// records allocated so stable handles remain inspectable, but removal_processed
// prevents them from participating in this path again.
[[nodiscard]] bool remove_legacy_group_member(
    EntityWorld& world,
    EntityGroupRuntime& group,
    EntityRuntime& member,
    bool destruction,
    bool player_attributed,
    LegacyRemovalContext& context,
    LegacyRandom& random,
    LegacyRemovalTrace& trace);

// PPC 0x36610 outer inactive-member pass. This adds the consequences that sit
// outside 0x36120 itself: obstacle creation, destroyed-child -> owner destruction,
// deletion spawns, then group/member removal. One call is one original-style
// traversal; a parent marked after its position in traversal is finalized by a
// later call, matching list-order behavior rather than recursively modernizing it.
[[nodiscard]] LegacyRemovalTrace finalize_legacy_pending_removals(
    EntityWorld& world,
    LegacyRemovalContext& context,
    LegacyRandom& random);

} // namespace deimos
