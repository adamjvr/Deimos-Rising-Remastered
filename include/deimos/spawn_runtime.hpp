#pragma once

#include "deimos/state_runtime.hpp"
#include "deimos/legacy_math.hpp"
#include "deimos/unit_definition.hpp"

#include <cstdint>
#include <optional>

namespace deimos {

// The original entity allocates one 24-byte runtime record for every spawn
// set in every state.  These fields map directly to the values touched by the
// 1.0.6 PPC routines at 0x15B40 and 0x17CB0.
struct SpawnSetRuntime {
    int rate_delay = 0;                 // +0x00
    std::uint32_t rate_anchor_tick = 0; // +0x04
    int remaining_in_volley = 0;        // +0x08
    int initial_volley_size = 0;        // +0x0C
    int inter_entity_delay = 0;         // +0x10
    bool active = false;                // +0x14
};

struct SpawnStateEntryResult {
    SpawnSetRuntime runtime;
    // 0x17CB0 writes this field to the parent entity's rotation-pause slot.
    // Canonical 1.0.6 data always uses zero, but preserving the write keeps
    // the clean model aligned with the executable.
    int rotation_pause_ticks = 0;
};

struct SpawnScheduleContext {
    bool parent_is_fleeing = false;
    // Result of PPC 0x16BD0.  It is consulted only at the beginning of a
    // volley when Don'tSpawnOffscreen is set.
    bool parent_is_onscreen = true;
    int current_rotation_pause_ticks = 0;
};

struct SpawnScheduleStep {
    bool spawn_due = false;
    bool deactivated = false;
    bool volley_cancelled_offscreen = false;
    bool rotation_pause_updated = false;
    int requested_rotation_pause_ticks = 0;
};

// Inputs used by the geometry tail of PPC 0x15B40.  The compiled Unit
// Definition parser at 0x3FDA0 maps #adjustInitialLocForOwnerScale_BOOL
// directly to UnitDef +0x12E, which is the byte tested by the spawn routine.
struct SpawnPlacementContext {
    float parent_x = 0.0f;
    float parent_y = 0.0f;
    float parent_scale = 1.0f;
    int parent_heading_degrees = 0;
    bool target_adjusts_initial_location_for_owner_scale = false;
};

// Target eligibility gate at PPC 0x15D8C..0x15DAC.  The Unit Definition
// parser maps #terrainEffect_BOOL to compiled UnitDef +0x132.  Terrain-effect
// units may be spawned only by a non-stationary parent whose own entity
// options have terrain effects enabled.
struct SpawnTargetEligibilityContext {
    bool target_is_terrain_effect = false;
    bool parent_is_stationary = false;
    bool parent_terrain_effects_enabled = false;
};

struct SpawnTargetProperties {
    bool terrain_effect = false;
    bool adjust_initial_location_for_owner_scale = false;
};

// A safe parent reference is stored by the original as a pointer plus the
// pointed-to member's monotonically increasing serial. PPC 0x36AB0 validates
// the pair before dereferencing the parent. The clean runtime uses a portable
// handle rather than a host pointer while preserving the serial contract.
using EntityReferenceHandle = std::uint64_t;
inline constexpr EntityReferenceHandle kNoEntityReferenceHandle = 0;

struct EntityReference {
    EntityReferenceHandle handle = kNoEntityReferenceHandle;
    std::uint32_t serial = 0;

    [[nodiscard]] bool empty() const { return handle == kNoEntityReferenceHandle; }
};

struct SpawnRequestContext {
    SpawnPlacementContext placement;
    bool parent_is_stationary = false;
    bool parent_terrain_effects_enabled = false;

    // PPC 0x15DB0..0x16188 copies these from the spawning live member into
    // request +0x14 and +0x20/+0x24 respectively.
    std::int8_t parent_player_owner_index = -1;
    EntityReference parent_reference{};
};

// Semantic representation of the exact 44-byte constructor request consumed
// by PPC 0x33220. The portable structure is not intentionally packed; comments
// record original byte offsets while clean code uses named fields.
struct SpawnRequestSeed {
    FourCC unit_id{};                         // +0x00
    float x = 0.0f;                          // +0x04
    float y = 0.0f;                          // +0x08
    bool subtract_world_y_origin = false;    // +0x0C
    bool heading_is_set = false;             // +0x0D
    int heading_degrees = 0;                 // +0x10
    std::int8_t player_owner_index = -1;     // +0x14
    int editor_heading_degrees = 0;          // +0x18
    bool stationary = false;                 // +0x1C
    bool terrain_effects_enabled = false;    // +0x1D
    EntityReference parent{};                // +0x20/+0x24
    float initial_velocity_multiplier = 1.0f;// +0x28
};

struct SpawnPlacement {
    float x = 0.0f;
    float y = 0.0f;
    bool heading_is_set = false;
    int heading_degrees = 0;
};

// Position/heading construction at PPC 0x15E18..0x16158.  Rotation uses the
// original 360-entry trig-table contract and PPC single-precision/fctiwz
// operation order.
[[nodiscard]] SpawnPlacement compute_spawn_placement(
    const UnitSpawnSet& spawn_set,
    const SpawnPlacementContext& context,
    const LegacyTrigTables& trig);

[[nodiscard]] bool spawn_target_is_eligible(
    const SpawnTargetEligibilityContext& context);

[[nodiscard]] SpawnTargetProperties spawn_target_properties(
    const UnitDefinition& target_definition);

[[nodiscard]] std::optional<SpawnRequestSeed> build_spawn_request_seed(
    const UnitSpawnSet& spawn_set,
    const UnitDefinition& target_definition,
    const SpawnRequestContext& context,
    const LegacyTrigTables& trig);

[[nodiscard]] bool is_none_spawn_id(const FourCC& id);

// PPC 0x17CB0.  RNG order is significant and differs from repeat re-arming:
// rate -> volley size -> per-entity delay.  "none" consumes no RNG.
[[nodiscard]] SpawnStateEntryResult initialize_spawn_set_runtime(
    const UnitSpawnSet& spawn_set,
    std::uint32_t current_tick,
    LegacyRandom& random);

// Scheduling-only portion of PPC 0x15B40.  This deliberately stops before
// target-unit eligibility, position/heading transformation and entity
// creation.  A true spawn_due means the original routine proceeds into that
// second half for this spawn set.
[[nodiscard]] SpawnScheduleStep advance_spawn_set_schedule(
    const UnitSpawnSet& spawn_set,
    SpawnSetRuntime& runtime,
    std::uint32_t current_tick,
    const SpawnScheduleContext& context,
    LegacyRandom& random);

// PPC 0x17150 pauses rotation while a volley is underway only after at least
// one entity has spawned and before the final entity has been emitted.
[[nodiscard]] bool spawn_set_is_mid_volley_for_rotation_pause(
    const UnitSpawnSet& spawn_set,
    const SpawnSetRuntime& runtime);

} // namespace deimos
