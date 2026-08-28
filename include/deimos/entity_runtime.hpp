#pragma once

#include "deimos/spawn_runtime.hpp"
#include "deimos/unit_behavior.hpp"
#include "deimos/unit_definition.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace deimos {

using EntityHandle = EntityReferenceHandle;
inline constexpr EntityHandle kNoEntityHandle = kNoEntityReferenceHandle;

// The original constructor (PPC 0x33220) does not create one live unit in all
// cases. It first chooses a group size with 0x369F0, applies constructor gates,
// creates a group/container, then 0x35BF0 creates the surviving live members.
struct EntityGroupSelection {
    int selected_before_appearance = 0;
    int surviving_members = 0;
    int appearance_rng_rolls = 0;
};

// Exact UnitDef-driven group selection at PPC 0x369F0. RNG consumption is
// part of the gameplay stream: a variable group-size consumes one draw, and
// each candidate with appearsPercent neither 0 nor 100 consumes a 0..100 roll.
[[nodiscard]] EntityGroupSelection select_entity_group_members(
    const UnitDefinition& unit,
    LegacyRandom& random);

// These three booleans correspond conservatively to the three runtime facts
// tested by PPC 0x332D8..0x33300 when the UnitDef carries
// canBeSpawnedOnlyWhenPlayersActive_BOOL. Their deeper game-mode semantics are
// intentionally not renamed until the surrounding player subsystem is mapped.
struct EntityPlayerGateFacts {
    bool global_gate_enabled = true;       // global byte r2-24860
    bool qualifying_player_present = true;// result of PPC 0x6110
    bool suppression_active = false;       // result of PPC 0x5CF0
};

struct EntityConstructionContext {
    std::uint32_t current_tick = 0;
    EntityPlayerGateFacts player_gate{};
    bool same_unit_type_already_exists = false; // result of PPC 0x36AF0
    int active_live_member_count = 0;            // global count before this request
};

enum class EntityConstructionRejection {
    none,
    no_group_members,
    player_gate_global_disabled,
    no_qualifying_player,
    player_gate_suppressed,
    duplicate_type_exists,
    live_member_limit
};

struct EntityGroupConstructionPlan {
    EntityConstructionRejection rejection = EntityConstructionRejection::none;
    EntityGroupSelection group{};

    // UnitDef deleteExistingEntitiesOfThisTypeOwnedByPlayer_BOOL causes
    // PPC 0x36BE0(unitID, request.player_owner_index, 0) after all rejection
    // gates, but only when the signed owner byte is not -1.
    bool delete_existing_owned_type = false;
    std::int8_t delete_existing_owner_index = -1;

    [[nodiscard]] bool accepted() const {
        return rejection == EntityConstructionRejection::none;
    }
};

// Constructor preflight through PPC 0x3338C. Crucially, group selection runs
// before the player/duplicate/cap gates, so rejected requests may consume RNG.
[[nodiscard]] EntityGroupConstructionPlan prepare_entity_group_construction(
    const UnitDefinition& unit,
    const SpawnRequestSeed& request,
    const EntityConstructionContext& context,
    LegacyRandom& random);


struct EntityPoint {
    float x = 0.0f;
    float y = 0.0f;
};

// Portable mirror of the proven fields initialized on the original 188-byte
// group/container at PPC 0x33454.  The clean object stores no host pointer or
// original intrusive-list internals.
struct EntityGroupRuntime {
    std::uint32_t serial = 0;
    FourCC unit_id{};
    EntityPoint base_position{};
    int member_count = 0;          // original group +0xA4: original member count
    int active_member_count = 0;   // original group +0xA8
    int destroyed_member_count = 0;// original group +0xAC
    int editor_heading_degrees = 0;
    bool stationary = false;
    bool terrain_effects_enabled = false;
};

// The original keeps independent monotonically increasing serials for group
// containers and live members.  A portable handle counter substitutes for the
// original member pointer while preserving the pointer+serial safe-reference
// contract.
struct EntityIdentityCounters {
    std::uint32_t next_group_serial = 0;
    std::uint32_t next_member_serial = 0;
    EntityHandle next_member_handle = 1;
};

struct EntityInitialPositionResult {
    EntityPoint position{};
    int rng_draws = 0;
};

// World facts needed only by branches proven to query another live object.
// Canonical 1.0.6 has one initially-hunting Unit Definition and no Burst/
// Implode definitions.  Keep these inputs explicit instead of burying world
// queries inside the deterministic constructor math.
struct EntityInitialMotionFacts {
    std::optional<EntityPoint> hunt_target_position;
    std::optional<int> parent_heading_degrees;
};

enum class EntityInitialMotionStatus {
    complete,
    missing_hunt_target,
    unsupported_burst_or_implode
};

struct EntityInitialMotionResult {
    EntityInitialMotionStatus status = EntityInitialMotionStatus::complete;
    float velocity_x = 0.0f;
    float velocity_y = 0.0f;

    // PPC live-member motion/target block. Direct disassembly of 1.0.6 maps
    // +0x108/+0x10C to target velocity, +0x110/+0x114 to per-axis velocity
    // delta, and +0x118/+0x11C/+0x120 to target-player identity/position.
    float target_velocity_x = 0.0f;
    float target_velocity_y = 0.0f;
    float velocity_delta_x = 0.0f;
    float velocity_delta_y = 0.0f;
    std::int8_t target_player_index = -1;
    float target_player_x = 0.0f;
    float target_player_y = 0.0f;
    float target_player_distance = 0.0f;
    bool has_active_target = false;

    // Live-member +0xCC selects PPC 0x16CC0, the recovered Flee motion path.
    // The transition that raises this runtime flag is kept separate until its
    // caller is fully mapped; canonical code can still execute the proven path.
    bool fleeing = false;

    int heading_degrees = 0;
    int rng_draws = 0;
};

struct EntityHeadlessConstructionContext {
    EntityConstructionContext preflight{};
    int world_y_origin = 0; // result of PPC 0xFEC0 for request +0x0C

    // PPC 0x5CD0 returns game-context +0x14. Its exact higher-level name is
    // still intentionally unresolved, but 0x35E64 uses (value - 1) as the
    // multiplier for shields_LevelIncrement_FLOAT. Default 1 preserves base
    // shields when constructing isolated/headless worlds.
    int shield_progression_value = 1;

    EntityInitialMotionFacts motion_facts{};

    // PPC's initially-hunting constructor query is position-dependent.  The
    // provider lets the world choose the closest active player separately for
    // each randomized group member while retaining motion_facts as a fallback
    // for isolated synthetic tests.
    std::function<std::optional<EntityPoint>(EntityPoint)> hunt_target_provider;
};

enum class EntityGroupBuildStatus {
    complete,
    rejected,
    missing_hunt_target,
    unsupported_burst_or_implode
};


// PPC 0x35FC8..0x35FF8 chooses one group delay for every member, including the
// first/final member, accumulates it, and stores the cumulative delay in the
// live member. This helper models that isolated operation; callers must place
// it in the full member-construction RNG sequence after state initialization.
[[nodiscard]] int append_group_member_delay(
    const UnitDefinition& unit,
    int accumulated_delay,
    LegacyRandom& random);

// Initial heading logic at PPC 0x35EB4..0x35F1C. If heading_mode is false, the
// UnitDef initialHeading_INT is used without RNG. If true, the supplied heading
// receives a symmetric +/- floor(initialHeadingTolerance/2) jitter.
[[nodiscard]] int choose_initial_member_heading(
    const UnitDefinition& unit,
    bool heading_mode,
    int supplied_heading_degrees,
    LegacyRandom& random);

enum class EntityLifecycle {
    active,
    deleted,
    destroyed
};

struct EntityStateSpawnRuntime {
    std::vector<SpawnSetRuntime> spawn_sets;
};

// Clean headless state-machine representation of one *live member*. The
// original distinguishes this from the 188-byte group/container allocated by
// PPC 0x33220. Identity fields mirror proven member fields where practical.
struct EntityRuntime {
    EntityHandle handle = kNoEntityHandle;
    std::uint32_t serial = 0;       // original live member +0x9C
    std::uint32_t group_serial = 0; // original live member +0xA0
    EntityReference parent{};       // original +0x140/+0x144 safe pair
    std::int8_t player_owner_index = -1; // original +0xD8
    FourCC unit_id{};

    float x = 0.0f;
    float y = 0.0f;
    float velocity_x = 0.0f;
    float velocity_y = 0.0f;

    // PPC live-member motion/target block. Direct disassembly of 1.0.6 maps
    // +0x108/+0x10C to target velocity, +0x110/+0x114 to per-axis velocity
    // delta, and +0x118/+0x11C/+0x120 to target-player identity/position.
    float target_velocity_x = 0.0f;
    float target_velocity_y = 0.0f;
    float velocity_delta_x = 0.0f;
    float velocity_delta_y = 0.0f;
    std::int8_t target_player_index = -1;
    float target_player_x = 0.0f;
    float target_player_y = 0.0f;
    float target_player_distance = 0.0f;
    bool has_active_target = false;

    // Live-member +0xCC selects PPC 0x16CC0, the recovered Flee motion path.
    // The transition that raises this runtime flag is kept separate until its
    // caller is fully mapped; canonical code can still execute the proven path.
    bool fleeing = false;

    int heading_degrees = 0;
    int group_delay_ticks = 0; // original live member +0xB0
    bool stationary = false;
    bool terrain_effects_enabled = false;

    // Collision/damage live-member fields mapped from PPC 0x12AD0,
    // 0x14F10 and the member constructor. The half extents (+0x2C/+0x30)
    // are populated by the sprite/appearance subsystem in the original; the
    // headless core leaves them at zero until geometry is supplied.
    int collision_half_width = 0;
    int collision_half_height = 0;
    bool collision_participating = true; // original live member +0xAC
    float shields = 0.0f;                // original +0x134
    std::int32_t last_collision_hit_tick = 0;       // original +0xB4
    std::int32_t last_on_hit_transition_tick = 0;   // original +0xFC
    std::int32_t last_collision_spawn_tick = 0;     // original +0xD0
    int collision_spawn_count = 0;                  // original +0xD4
    bool consumed_as_player_pickup = false;          // original +0xCA
    std::int8_t destroyed_by_owner_index = -1;      // original +0xD9

    // Clean-only bookkeeping for the two-stage legacy teardown. The original
    // marks destruction at +0xCB/+0xDA, then later PPC 0x36120 removes the
    // live member from its group/list and updates counters. Existing clean
    // callers can mark lifecycle=destroyed before this teardown stage.
    bool destruction_effects_processed = false;
    bool removal_processed = false;

    // PPC 0x33600 / 0x37130 / 0x37230 / 0x37350 owner-location
    // bookkeeping. These mirror live-member +0x124..+0x130 and the Orbit
    // radius/angle at +0xDC/+0xE0.  The state marker is clean-only and lets
    // the world layer re-run 0x33600 semantics after every state change.
    float owner_offset_x = 0.0f;
    float owner_offset_y = 0.0f;
    float previous_owner_x = 0.0f;
    float previous_owner_y = 0.0f;
    float orbit_radius = 0.0f;
    int orbit_angle_degrees = 0;
    std::optional<std::size_t> owner_location_initialized_state;

    EntityLifecycle lifecycle = EntityLifecycle::active;
    CompiledUnitBehavior behavior;
    UnitStateRuntime state;
    std::vector<EntityStateSpawnRuntime> spawn_runtime_by_state;
    int rotation_pause_ticks = 0;
};

[[nodiscard]] EntityGroupRuntime build_entity_group_runtime(
    const SpawnRequestSeed& request,
    int member_count,
    std::uint32_t group_serial,
    int world_y_origin);

// PPC 0x37930 initial placement math.  This includes the original asymmetry:
// when both axes vary it first chooses an angle and optionally a radial random
// distance; otherwise each varying axis is truncated to integer endpoints and
// uses the signed inclusive integer RNG helper.
[[nodiscard]] EntityInitialPositionResult choose_initial_member_position(
    const UnitDefinition& unit,
    const EntityGroupRuntime& group,
    LegacyRandom& random,
    const LegacyTrigTables& trig);

// PPC 0x37B50 initial velocity/orientation subset.  Heading-mode values passed
// from the request/editor have already received the member-constructor
// tolerance jitter via choose_initial_member_heading().
[[nodiscard]] EntityInitialMotionResult choose_initial_member_motion(
    const UnitDefinition& unit,
    const EntityGroupRuntime& group,
    const EntityPoint& member_position,
    bool stationary,
    bool heading_mode,
    int preselected_heading_degrees,
    float velocity_multiplier,
    const EntityInitialMotionFacts& facts,
    LegacyRandom& random,
    const LegacyTrigTables& trig);

[[nodiscard]] bool unit_requires_active_players(const UnitDefinition& unit);

// Exact shield initializer from PPC 0x35E50..0x35EB0. Scaling and max-clamp
// occur only when shields_LevelIncrement_FLOAT > 0.0; an increment <= 0 copies
// shields_BaseAmount_FLOAT verbatim and does not apply shields_MaxAmount_FLOAT.
[[nodiscard]] float legacy_initial_entity_shields(
    const CompiledUnitBehavior& behavior,
    int shield_progression_value);

// Enter a live entity state using the original state-entry transition and
// spawn-set initialization paths. This owns RNG consumption; callers must not
// pre-draw a value. Counter-triggered state changes are followed immediately.
void enter_entity_state(
    EntityRuntime& entity,
    const UnitDefinition& unit,
    std::size_t state_index,
    std::uint32_t current_tick,
    LegacyRandom& random);

// Initialize only the independently recovered headless state-machine portion
// of an already-created member. This is deliberately NOT named as PPC 0x33220
// construction: group selection, position, velocity, shields and world-list
// insertion are separate constructor stages still being reconstructed.
void initialize_entity_state_machine(
    EntityRuntime& entity,
    const UnitDefinition& unit,
    std::uint32_t current_tick,
    LegacyRandom& random);

// Apply one already-resolved action. State changes call enter_entity_state,
// Delete/Destroy mark lifecycle, and unresolved labels remain runtime no-ops.
void apply_entity_state_action(
    EntityRuntime& entity,
    const UnitDefinition& unit,
    const ResolvedStateAction& action,
    std::uint32_t current_tick,
    LegacyRandom& random);

struct EntityGroupBuildResult {
    EntityGroupBuildStatus status = EntityGroupBuildStatus::rejected;
    EntityGroupConstructionPlan plan{};
    std::optional<EntityGroupRuntime> group;
    std::vector<EntityRuntime> members;
    EntityReference first_member_reference{};

    [[nodiscard]] bool constructed() const {
        return status == EntityGroupBuildStatus::complete;
    }
};

// Faithful headless normal-path bridge from PPC 0x33220 -> 0x35BF0 -> 0x35CD0.
// It models group selection/gates, normal 188-byte group identity, initial
// heading/position/motion, level-scaled shields, state-entry/spawn-runtime
// initialization and cumulative group delay in original RNG order. Intrusive
// world-list insertion, render state and the rare special single-member parent
// container path remain separate reconstruction work.
[[nodiscard]] EntityGroupBuildResult construct_entity_group_headless(
    const UnitDefinition& unit,
    const SpawnRequestSeed& request,
    const EntityHeadlessConstructionContext& context,
    EntityIdentityCounters& identities,
    LegacyRandom& random,
    const LegacyTrigTables& trig);


// Motion primitives mapped from PPC 0x146F0 / 0x16FE0 / 0x17A10 / 0x17B70 /
// 0x17C40 / 0x16CC0. They deliberately operate only on the proven live-member
// motion block and state fields so they remain deterministic and testable.
void initialize_entity_state_motion(
    EntityRuntime& entity,
    const UnitDefinition& unit,
    std::optional<std::size_t> previous_state_index = std::nullopt);

void advance_entity_hunt_motion(
    EntityRuntime& entity,
    const UnitDefinition& unit,
    LegacyRandom& random);

void advance_entity_hold_motion(
    EntityRuntime& entity,
    const UnitDefinition& unit);

void advance_entity_cyclic_motion(
    EntityRuntime& entity,
    const UnitDefinition& unit);

void advance_entity_flee_motion(
    EntityRuntime& entity,
    const UnitDefinition& unit);

void converge_entity_velocity(
    EntityRuntime& entity,
    const UnitDefinition& unit);

struct EntityTickContext {
    std::uint32_t current_tick = 0;

    // Facts are sampled after the original animation-update phase and before
    // the five rule slots. A missing provider skips rule evaluation while
    // animation/world reconstruction is still incomplete.
    UnitRuleFactsProvider facts_for_rule;

    // Range transition is evaluated later than rules. A missing measurement
    // means the world layer has not supplied a player-distance result yet.
    std::optional<float> measured_player_range;

    // PPC 0x15280 refreshes the target-player facts and executes the pre-range
    // Hunt/no-player portion after rules but before the range transition.  The
    // callback can return the just-measured player range for that exact tick.
    std::function<std::optional<float>(EntityRuntime&)> pre_range_motion_phase;

    // After a possible range state change, 0x15280 reloads the current state and
    // executes Hold/Cyclic/Flee/convergence work.  Keep this separate from owner
    // Lock/Link/Orbit, which the caller performs immediately afterward.
    std::function<void(EntityRuntime&)> post_range_motion_phase;

    // Main member update 0x3401C..0x34054 executes Lock/Link/Orbit owner
    // location behavior after range handling and before spawn scheduling. The
    // portable world layer installs this phase without coupling the core state
    // interpreter to a particular world/container implementation.
    std::function<void(EntityRuntime&)> owner_location_phase;

    SpawnScheduleContext spawn_schedule;
};

struct EntityTickSpawnEvent {
    std::size_t state_index = 0;
    std::size_t spawn_set_index = 0;
};

struct EntityTickResult {
    bool timer_action_processed = false;
    bool rule_matched = false;
    bool range_action_processed = false;
    std::vector<EntityTickSpawnEvent> spawns_due;
};

// Recovered headless subset of the 1.0.6 per-member update order:
// timer -> animation/world facts -> first matching rule -> target/Hunt/no-player
// phase -> range -> Hold/Cyclic/Flee/convergence -> owner Lock/Link/Orbit ->
// spawn scheduling. State actions refresh current state immediately, so later
// phases in the same tick observe the new state.
[[nodiscard]] EntityTickResult advance_entity_runtime(
    EntityRuntime& entity,
    const UnitDefinition& unit,
    const EntityTickContext& context,
    LegacyRandom& random);

} // namespace deimos
