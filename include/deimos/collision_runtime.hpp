#pragma once

#include "deimos/entity_world.hpp"
#include "deimos/particle_runtime.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace deimos {

struct LegacyRemovalContext;
struct LegacyRemovalTrace;

// Integer AABB produced by PPC 0x12AD0. The original converts each edge with
// fctiwz, so C++ truncation toward zero is the relevant portable operation.
struct LegacyCollisionBounds {
    int min_x = 0;
    int min_y = 0;
    int max_x = 0;
    int max_y = 0;
};

[[nodiscard]] LegacyCollisionBounds legacy_collision_bounds(const EntityRuntime& entity);
[[nodiscard]] bool legacy_collision_bounds_overlap(
    const LegacyCollisionBounds& a,
    const LegacyCollisionBounds& b);

// PPC 0x36F60..0x36FB8 derives one radius from each already-truncated AABB
// as trunc((maxY-minY)/2), then PPC 0x42F80 compares a quantized center
// distance against the sum. 0x429C0..0x42A00 proves the small-distance table
// used by 0x42F80 contains sqrt(i) for i=0..16383. The distance input itself
// is fctiwz(dx*dx + dy*dy), so fractional squared distance is discarded before
// sqrt and equality is NOT a collision.
[[nodiscard]] int legacy_collision_radius(const LegacyCollisionBounds& bounds);
[[nodiscard]] float legacy_quantized_center_distance(
    float x1, float y1, float x2, float y2);
[[nodiscard]] bool legacy_radial_collision(
    const EntityRuntime& first,
    const LegacyCollisionBounds& first_bounds,
    const EntityRuntime& second,
    const LegacyCollisionBounds& second_bounds);

// Player collision geometry from PPC 0x12A00 + 0x33968..0x341C8. Unlike
// entity/entity radius construction, the player radius is 0.5 * the complete
// integer Rect height and may therefore be a half-integer. The entity radius
// remains signed integer division by two.
[[nodiscard]] LegacyCollisionBounds legacy_player_collision_bounds(
    const PlayerRuntimeSlot& player);
[[nodiscard]] float legacy_player_collision_radius(
    const LegacyCollisionBounds& player_bounds);
[[nodiscard]] bool legacy_entity_player_geometry_overlap(
    const EntityRuntime& entity,
    const LegacyCollisionBounds& entity_bounds,
    const PlayerRuntimeSlot& player,
    const LegacyCollisionBounds& player_bounds);

// Damage can redirect through a safe owner reference and can trigger a state
// transition, so the runtime needs the matching Unit Definition for any live
// member that becomes the actual damage target.
using CollisionUnitDefinitionProvider =
    std::function<const UnitDefinition*(FourCC unit_id)>;

struct CollisionDamageResult {
    bool applied = false;
    float shields_before = 0.0f;
    float shields_after = 0.0f;
    float absorbed_damage = 0.0f;
    bool invulnerability_restored_shields = false;
    bool on_hit_action_due = false;
    bool entity_destroyed = false;
    bool shield_depletion_state_entered = false;
    std::optional<std::size_t> shield_depletion_state_index;
    bool collision_glow_due = false;
    bool hit_particles_due = false;
    std::optional<LegacyParticleSpawnRequest> hit_particle_spawn;
    bool hit_particle_executed = false;
    int score_award = 0;
    std::optional<FourCC> collision_spawn_due;
};

// Main entity tick 0x340BC..0x340E8 applies a viewport guard before building
// the per-player collision loop. The left edge deliberately admits 32 pixels
// offscreen; the top edge does not. max_x/max_y correspond to the two runtime
// viewport limits held in r24/r23 by the original caller.
struct LegacyPlayerCollisionViewport {
    int max_x = 0;
    int max_y = 0;
};

[[nodiscard]] bool legacy_entity_within_player_collision_viewport(
    const LegacyCollisionBounds& entity_bounds,
    const LegacyPlayerCollisionViewport& viewport);

// 0x37580 is the player pickup dispatcher and 0x27100 owns player shield/damage
// semantics. Concrete clean mutations live in player_runtime.cpp; collision
// keeps explicit callbacks so returned spawn/audio/UI consequences can be
// orchestrated by the world layer without coupling them to geometry. The
// collision loop preserves the dispatcher Boolean and re-reads player.status
// after damage just like 0x26C50.
using LegacyPlayerPickupHandler =
    std::function<bool(PlayerRuntimeSlot& player, const EntityRuntime& pickup_entity)>;
using LegacyPlayerDamageHandler =
    std::function<void(PlayerRuntimeSlot& player, float damage, std::uint32_t current_tick)>;

struct LegacyPlayerCollisionCallbacks {
    LegacyPlayerPickupHandler try_pickup;
    LegacyPlayerDamageHandler apply_player_damage;
};

struct PlayerCollisionEvent {
    std::size_t player_slot = 0;
    std::int8_t player_index = -1;
    bool pickup_attempted = false;
    bool pickup_consumed = false;
    EntityHandle entity_damage_target = kNoEntityHandle;
    CollisionDamageResult entity_damage;
    float player_damage_requested = 0.0f;
    bool player_remained_active = true;
};

struct PlayerCollisionScanResult {
    std::size_t players_considered = 0;
    std::size_t aabb_overlaps = 0;
    std::size_t radial_overlaps = 0;
    std::size_t pickup_attempts = 0;
    std::size_t pickup_consumptions = 0;
    std::size_t reciprocal_impacts = 0;
    bool entity_became_inactive = false;
    std::vector<PlayerCollisionEvent> events;
};

// Bounded clean reconstruction of the player-impact section of the main
// entity update, PPC 0x34090..0x34314. Entity-side damage uses Game.gafl index
// 161 (Player_ImpactDamageToEntities; canonical 1.0.6 = 100.0). Non-'none'
// pickup types route exclusively through 0x37580 semantics: failed pickups do
// nothing further; successful pickups destroy/consume the entity and skip
// reciprocal impact damage. Ordinary impacts optionally redirect entity damage
// through passHitsToOwner, then always request player damage from UnitDef
// damage_FLOAT, even if the entity/owner was destroyed by the first leg.
[[nodiscard]] PlayerCollisionScanResult scan_legacy_player_collisions(
    EntityWorld& world,
    EntityRuntime& entity,
    PlayerWorld& players,
    const LegacyPlayerCollisionViewport& viewport,
    std::uint32_t current_tick,
    const CollisionUnitDefinitionProvider& definition_for_unit,
    LegacyRandom& random,
    const LegacyPlayerCollisionCallbacks& callbacks = {},
    float player_impact_damage_to_entities = 100.0f,
    int entity_hit_delay_ticks = 1,
    LegacyRemovalContext* removal_context = nullptr,
    LegacyRemovalTrace* removal_trace = nullptr);

// PPC 0x14F10. `entity_hit_delay_ticks` is the value read from Game.gafl
// index 167 (canonical 1.0.6: Entity_HitDelay = 1.0, truncated to 1).
[[nodiscard]] CollisionDamageResult apply_collision_damage(
    EntityRuntime& target,
    const UnitDefinition& target_definition,
    float damage,
    std::int8_t source_owner_index,
    std::uint32_t current_tick,
    LegacyRandom& random,
    int entity_hit_delay_ticks = 1,
    LegacyRemovalContext* removal_context = nullptr,
    LegacyRemovalTrace* removal_trace = nullptr);

// Non-geometry candidate gates from PPC 0x36CF0. This intentionally includes
// the executable's asymmetric projectile policy and the opposing
// harmlessToPlayers classes; it does not perform the AABB/radial tests.
[[nodiscard]] bool legacy_collision_candidate_compatible(
    const EntityRuntime& self,
    const EntityRuntime& candidate);

struct CollisionPairResult {
    bool collided = false;
    EntityHandle first_damage_target = kNoEntityHandle;
    EntityHandle second_damage_target = kNoEntityHandle;
    std::int8_t first_damage_source_owner_index = -1;
    std::int8_t second_damage_source_owner_index = -1;
    CollisionDamageResult first_damage;
    CollisionDamageResult second_damage;
};

// Apply the symmetric damage tail of PPC 0x36FC4..0x370EC after the radial
// collision test has succeeded. The second passHitsToOwner leg deliberately
// preserves the 1.0.6 binary's use of *self's* parent safe-reference rather
// than candidate.parent.
[[nodiscard]] CollisionPairResult apply_legacy_collision_pair(
    EntityWorld& world,
    EntityRuntime& self,
    EntityRuntime& candidate,
    std::uint32_t current_tick,
    const CollisionUnitDefinitionProvider& definition_for_unit,
    LegacyRandom& random,
    int entity_hit_delay_ticks = 1,
    LegacyRemovalContext* removal_context = nullptr,
    LegacyRemovalTrace* removal_trace = nullptr);

struct CollisionScanResult {
    std::size_t candidates_considered = 0;
    std::size_t aabb_overlaps = 0;
    std::size_t radial_overlaps = 0;
    std::size_t collisions_applied = 0;
    bool self_became_inactive = false;

    // Preserve each successful pair result in exact traversal order.  The
    // aggregate scanner previously discarded these facts, which made
    // collisionSpawn_ID requests produced by PPC 0x14F10 unreachable from
    // the owning world loop even though the pair-damage runtime recovered
    // them correctly.
    std::vector<CollisionPairResult> pairs;
};

// Bounded headless reconstruction of PPC 0x36CF0. EntityWorld insertion order
// preserves the original group/member traversal order for the current clean
// world model. The scan exits immediately after a collision makes `self`
// inactive, matching 0x370F0..0x37114.
[[nodiscard]] CollisionScanResult scan_legacy_entity_collisions(
    EntityWorld& world,
    EntityRuntime& self,
    std::uint32_t current_tick,
    const CollisionUnitDefinitionProvider& definition_for_unit,
    LegacyRandom& random,
    int entity_hit_delay_ticks = 1,
    LegacyRemovalContext* removal_context = nullptr,
    LegacyRemovalTrace* removal_trace = nullptr);

} // namespace deimos
