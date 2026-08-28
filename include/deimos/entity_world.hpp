#pragma once

#include "deimos/entity_runtime.hpp"
#include "deimos/legacy_math.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace deimos {


// The original player target subsystem owns exactly two slots. PPC 0x5D40 /
// 0x5ED0 / 0x6090 / 0x6110 treats status byte 4 as active and returns the
// player's signed index byte rather than assuming the slot number is identity.
struct PlayerRuntimeSlot {
    int status = 0;
    float x = 0.0f;
    float y = 0.0f;
    std::int8_t player_index = -1;
};

struct ClosestPlayerResult {
    std::size_t slot = 0;
    std::int8_t player_index = -1;
    EntityPoint position{};
    float distance = 0.0f;
};

class PlayerWorld {
public:
    static constexpr std::size_t kPlayerSlots = 2;

    [[nodiscard]] const std::array<PlayerRuntimeSlot, kPlayerSlots>& slots() const { return slots_; }
    [[nodiscard]] std::array<PlayerRuntimeSlot, kPlayerSlots>& slots() { return slots_; }

    [[nodiscard]] bool any_active_player() const;
    [[nodiscard]] std::optional<EntityPoint> position_for_player_index(std::int8_t player_index) const;
    [[nodiscard]] std::optional<ClosestPlayerResult> closest_active_player(float x, float y) const;

private:
    std::array<PlayerRuntimeSlot, kPlayerSlots> slots_{};
};

// Portable world registry for the recovered live-member identity contract.
// The original uses intrusive lists and raw pointers; clean code uses stable
// numeric handles while preserving serial validation and lifecycle checks.
class EntityWorld {
public:
    [[nodiscard]] const std::vector<EntityGroupRuntime>& groups() const { return groups_; }
    [[nodiscard]] std::vector<EntityGroupRuntime>& groups() { return groups_; }
    [[nodiscard]] const std::vector<EntityRuntime>& members() const { return members_; }
    [[nodiscard]] std::vector<EntityRuntime>& members() { return members_; }

    void register_group(EntityGroupBuildResult&& build);

    [[nodiscard]] EntityRuntime* find_member(EntityHandle handle);
    [[nodiscard]] const EntityRuntime* find_member(EntityHandle handle) const;

    // PPC 0x36AB0: pointer/handle must exist, serial must match live +0x9C,
    // and live +0xCB must still represent an active member.
    [[nodiscard]] EntityRuntime* resolve_reference(const EntityReference& reference);
    [[nodiscard]] const EntityRuntime* resolve_reference(const EntityReference& reference) const;

    // PPC 0x36AF0 duplicate query: first active member with matching Unit ID.
    [[nodiscard]] EntityRuntime* find_first_active_unit(FourCC unit_id);
    [[nodiscard]] const EntityRuntime* find_first_active_unit(FourCC unit_id) const;
    [[nodiscard]] bool has_active_unit(FourCC unit_id) const;
    [[nodiscard]] std::size_t active_member_count() const;

    // PPC 0x36BE0 scans matching Unit ID + signed owner index. The original
    // then enters its member-removal path. Until death-effect semantics are
    // reconstructed, this clean world performs only the proven world-visible
    // result: matching members stop participating as active entities.
    [[nodiscard]] std::size_t mark_owned_unit_deleted(
        FourCC unit_id,
        std::int8_t player_owner_index);

private:
    std::vector<EntityGroupRuntime> groups_;
    std::vector<EntityRuntime> members_;
};

using PlayerPositionProvider =
    std::function<std::optional<EntityPoint>(std::int8_t player_owner_index)>;

// Original owner resolution order used by 0x33600 and all three owner-location
// update routines: valid safe parent first, then player-owner position.
[[nodiscard]] std::optional<EntityPoint> resolve_entity_owner_position(
    const EntityWorld& world,
    const EntityRuntime& entity,
    const PlayerPositionProvider& player_position);

enum class EntityOwnerLocationMode {
    none,
    lock_to_owner_location,
    link_to_owner_location,
    orbit_owner
};

[[nodiscard]] EntityOwnerLocationMode current_owner_location_mode(
    const UnitDefinition& unit,
    const EntityRuntime& entity);

// PPC 0x33600. Called after construction and after state changes. It initializes
// the owner-position history and mode-specific relative/orbit bookkeeping.
// Returns false when the state has no owner mode or no valid owner position.
[[nodiscard]] bool initialize_entity_owner_location(
    EntityRuntime& entity,
    const UnitDefinition& unit,
    const std::optional<EntityPoint>& owner_position);

// PPC 0x37130 / 0x37230 / 0x37350. This function assumes 0x33600 semantics
// have run for the current state (it will do so lazily when needed).
// Orbit uses int(trunc(live velocity X)) as the original angular step source.
[[nodiscard]] bool advance_entity_owner_location(
    EntityRuntime& entity,
    const UnitDefinition& unit,
    const std::optional<EntityPoint>& owner_position,
    const LegacyTrigTables& trig);

// Convenience world-facing phase: resolve parent/player owner, lazily perform
// 0x33600 for a newly entered state, then run the selected owner mode.
[[nodiscard]] bool advance_entity_owner_location_from_world(
    EntityWorld& world,
    EntityRuntime& entity,
    const UnitDefinition& unit,
    const PlayerPositionProvider& player_position,
    const LegacyTrigTables& trig);


// Player-aware wrapper over the recovered 0x15280 motion/target dispatcher.
// It refreshes the two-slot closest-player facts in the exact pre-range phase,
// applies Delete/Destruct-on-no-player, Hunt, then post-range Hold/Cyclic/Flee
// and velocity convergence before the existing owner-location/spawn phases.
[[nodiscard]] EntityTickResult advance_entity_runtime_with_players(
    EntityWorld& world,
    EntityRuntime& entity,
    const UnitDefinition& unit,
    const EntityTickContext& context,
    const PlayerWorld& players,
    LegacyRandom& random,
    const LegacyTrigTables& trig);

// World-aware wrapper over advance_entity_runtime(). It installs the recovered
// owner-location phase in the exact range -> owner mode -> spawn-scheduler
// position while keeping rule/range fact providers explicit.
[[nodiscard]] EntityTickResult advance_entity_runtime_in_world(
    EntityWorld& world,
    EntityRuntime& entity,
    const UnitDefinition& unit,
    const EntityTickContext& context,
    const PlayerPositionProvider& player_position,
    LegacyRandom& random,
    const LegacyTrigTables& trig);

} // namespace deimos
