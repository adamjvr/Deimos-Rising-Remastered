#include "deimos/collision_runtime.hpp"

#include "deimos/destruction_runtime.hpp"
#include "deimos/spawn_runtime.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace deimos {
namespace {

constexpr FourCC kGroundDomain{{'g', 'r', 'n', 'd'}};

bool active(const EntityRuntime& entity) {
    return entity.lifecycle == EntityLifecycle::active;
}

const CompiledUnitStateBehavior& current_state(const EntityRuntime& entity) {
    if (entity.state.current_state >= entity.behavior.states.size()) {
        throw std::out_of_range("collision runtime current state outside compiled behavior");
    }
    return entity.behavior.states[entity.state.current_state];
}

const UnitDefinition& definition_or_throw(
    const CollisionUnitDefinitionProvider& provider,
    FourCC unit_id) {
    if (!provider) throw std::invalid_argument("collision Unit Definition provider is empty");
    const auto* definition = provider(unit_id);
    if (!definition) {
        throw std::invalid_argument("collision Unit Definition provider has no entry for " + unit_id.str());
    }
    return *definition;
}

std::int32_t wrapping_add(std::int32_t base, int delta) {
    const auto sum = static_cast<std::uint32_t>(base) + static_cast<std::uint32_t>(delta);
    return static_cast<std::int32_t>(sum);
}

bool fourcc_is_none_or_empty(FourCC id) {
    return id == FourCC{} || id.str() == "none";
}

EntityRuntime* first_damage_target(EntityWorld& world, EntityRuntime& self) {
    if (current_state(self).pass_hits_to_owner) {
        if (auto* parent = world.resolve_reference(self.parent)) return parent;
    }
    return &self;
}

EntityRuntime* second_damage_target(
    EntityWorld& world,
    EntityRuntime& self,
    EntityRuntime& candidate) {
    if (current_state(candidate).pass_hits_to_owner) {
        // PPC 1.0.6 0x37074 loads live-self +0x140/+0x144 here, not the
        // candidate's safe parent pair. Preserve that observable quirk.
        if (auto* parent = world.resolve_reference(self.parent)) return parent;
    }
    return &candidate;
}

} // namespace

LegacyCollisionBounds legacy_collision_bounds(const EntityRuntime& entity) {
    // PPC 0x12AD0 converts the integer half extents to float, then applies
    // fctiwz independently to x-/+halfWidth and y-/+halfHeight.
    const float half_width = static_cast<float>(entity.collision_half_width);
    const float half_height = static_cast<float>(entity.collision_half_height);
    return {
        static_cast<int>(std::trunc(static_cast<float>(entity.x - half_width))),
        static_cast<int>(std::trunc(static_cast<float>(entity.y - half_height))),
        static_cast<int>(std::trunc(static_cast<float>(entity.x + half_width))),
        static_cast<int>(std::trunc(static_cast<float>(entity.y + half_height)))
    };
}

bool legacy_collision_bounds_overlap(
    const LegacyCollisionBounds& a,
    const LegacyCollisionBounds& b) {
    // 0x36EF8..0x36F34 rejects only strict separation, so touching edges are
    // an AABB overlap and proceed to the radial center-distance test.
    if (a.max_y < b.min_y) return false;
    if (a.min_y > b.max_y) return false;
    if (a.max_x < b.min_x) return false;
    if (a.min_x > b.max_x) return false;
    return true;
}

int legacy_collision_radius(const LegacyCollisionBounds& bounds) {
    // PPC 0x36F60..0x36F8C uses the standard signed divide-by-two sequence
    // (sign adjust + srawi), i.e. truncation toward zero. A well-formed AABB
    // has a non-negative vertical span.
    return (bounds.max_y - bounds.min_y) / 2;
}

float legacy_quantized_center_distance(float x1, float y1, float x2, float y2) {
    // 0x42F9C..0x42FBC is single-precision subtract/multiply/fused-add and
    // fctiwz. Reproduce the fused second term explicitly before truncating.
    const float dy = static_cast<float>(y1 - y2);
    const float dx = static_cast<float>(x1 - x2);
    const float dy_squared = static_cast<float>(dy * dy);
    const float squared = std::fma(dx, dx, dy_squared);
    const int quantized_squared = static_cast<int>(std::trunc(squared));

    // 0x429C0..0x42A00 initializes the <16384 lookup table with sqrt(i);
    // >=16384 goes directly through sqrt and rounds back to single. Both paths
    // are therefore represented by sqrt(double(integer)) -> float here.
    return static_cast<float>(std::sqrt(static_cast<double>(quantized_squared)));
}

bool legacy_radial_collision(
    const EntityRuntime& first,
    const LegacyCollisionBounds& first_bounds,
    const EntityRuntime& second,
    const LegacyCollisionBounds& second_bounds) {
    const float first_radius = static_cast<float>(legacy_collision_radius(first_bounds));
    const float second_radius = static_cast<float>(legacy_collision_radius(second_bounds));
    const float distance = legacy_quantized_center_distance(
        first.x, first.y, second.x, second.y);
    // 0x43010 returns CR[LT], so equality is explicitly not a hit.
    return distance < static_cast<float>(first_radius + second_radius);
}

LegacyCollisionBounds legacy_player_collision_bounds(const PlayerRuntimeSlot& player) {
    // PPC 0x12A00 writes a classic Rect as top,left,bottom,right, but expose
    // the same semantic min/max ordering used by the clean entity helper.
    const float half_width = static_cast<float>(player.collision_half_width);
    const float half_height = static_cast<float>(player.collision_half_height);
    return {
        static_cast<int>(std::trunc(static_cast<float>(player.x - half_width))),
        static_cast<int>(std::trunc(static_cast<float>(player.y - half_height))),
        static_cast<int>(std::trunc(static_cast<float>(player.x + half_width))),
        static_cast<int>(std::trunc(static_cast<float>(player.y + half_height)))
    };
}

float legacy_player_collision_radius(const LegacyCollisionBounds& player_bounds) {
    // 0x33968..0x3399C converts (Rect.bottom - Rect.top) to single then
    // multiplies by the startup constant at TOC -28400, proven to be 0.5f.
    return static_cast<float>(
        static_cast<float>(player_bounds.max_y - player_bounds.min_y) * 0.5f);
}

bool legacy_entity_player_geometry_overlap(
    const EntityRuntime& entity,
    const LegacyCollisionBounds& entity_bounds,
    const PlayerRuntimeSlot& player,
    const LegacyCollisionBounds& player_bounds) {
    if (!legacy_collision_bounds_overlap(entity_bounds, player_bounds)) return false;
    const float player_radius = legacy_player_collision_radius(player_bounds);
    const float entity_radius = static_cast<float>(legacy_collision_radius(entity_bounds));
    const float distance = legacy_quantized_center_distance(
        player.x, player.y, entity.x, entity.y);
    return distance < static_cast<float>(player_radius + entity_radius);
}

bool legacy_entity_within_player_collision_viewport(
    const LegacyCollisionBounds& entity_bounds,
    const LegacyPlayerCollisionViewport& viewport) {
    // Main tick 0x340BC..0x340E8 tests maxX >= -32, minX <= r24,
    // maxY >= 0, minY <= r23. These are strict outside rejections, so edges
    // exactly on the limits remain eligible.
    if (entity_bounds.max_x < -32) return false;
    if (entity_bounds.min_x > viewport.max_x) return false;
    if (entity_bounds.max_y < 0) return false;
    if (entity_bounds.min_y > viewport.max_y) return false;
    return true;
}

PlayerCollisionScanResult scan_legacy_player_collisions(
    EntityWorld& world,
    EntityRuntime& entity,
    PlayerWorld& players,
    const LegacyPlayerCollisionViewport& viewport,
    std::uint32_t current_tick,
    const CollisionUnitDefinitionProvider& definition_for_unit,
    LegacyRandom& random,
    const LegacyPlayerCollisionCallbacks& callbacks,
    float player_impact_damage_to_entities,
    int entity_hit_delay_ticks,
    LegacyRemovalContext* removal_context,
    LegacyRemovalTrace* removal_trace) {
    PlayerCollisionScanResult result;
    if (!active(entity)) return result;
    if (!players.any_active_player()) return result;

    const auto& state = current_state(entity);
    if (!state.collides || entity.behavior.harmless_to_players ||
        !state.collides_with_players) {
        return result;
    }

    const auto entity_bounds = legacy_collision_bounds(entity);
    if (!legacy_entity_within_player_collision_viewport(entity_bounds, viewport)) {
        return result;
    }

    // 0x33850..0x339A4 snapshots the two active flags, player Rects, centers,
    // and radii before the member-update collision loop. Build the equivalent
    // geometry once here; player status may change after 0x27100 later.
    struct Snapshot {
        bool active = false;
        LegacyCollisionBounds bounds{};
    };
    std::array<Snapshot, PlayerWorld::kPlayerSlots> snapshots{};
    for (std::size_t i = 0; i < snapshots.size(); ++i) {
        const auto& player = players.slots()[i];
        snapshots[i].active = player.status == 4;
        if (snapshots[i].active) {
            snapshots[i].bounds = legacy_player_collision_bounds(player);
        }
    }

    for (std::size_t i = 0; i < snapshots.size(); ++i) {
        // 0x34110 rechecks the entity destroyed byte at the top of each player
        // iteration. A successful pickup or lethal first-slot impact therefore
        // prevents a second-slot collision, but reciprocal player damage for
        // the lethal impact has already happened before this next check.
        if (!active(entity)) break;
        if (!snapshots[i].active) continue;

        auto& player = players.slots()[i];
        ++result.players_considered;
        if (!legacy_collision_bounds_overlap(entity_bounds, snapshots[i].bounds)) continue;
        ++result.aabb_overlaps;
        if (!legacy_entity_player_geometry_overlap(
                entity, entity_bounds, player, snapshots[i].bounds)) {
            continue;
        }
        ++result.radial_overlaps;

        PlayerCollisionEvent event;
        event.player_slot = i;
        event.player_index = player.player_index;

        if (!fourcc_is_none_or_empty(entity.behavior.pickup_type)) {
            // 0x341D0..0x34224: pickup entities never fall through to normal
            // impact damage. A failed dispatcher result simply ends this slot;
            // a successful result invokes destruction with the player's owner
            // byte and sets the entity-side consumed/destruction state.
            event.pickup_attempted = true;
            ++result.pickup_attempts;
            const bool consumed = callbacks.try_pickup
                ? callbacks.try_pickup(player, entity)
                : false;
            event.pickup_consumed = consumed;
            if (consumed) {
                ++result.pickup_consumptions;
                // PPC 0x34214 calls 0x16300 immediately, before +0xCA is set.
                // When the caller supplies the destruction context/trace, run
                // those effects in-place so any random-bonus RNG draw remains
                // in exact collision-loop order. The bounded fallback retains
                // the previously proven lifecycle result.
                if (removal_context && removal_trace) {
                    (void)apply_legacy_destruction_effects(
                        entity, player.player_index, *removal_context, random, *removal_trace);
                } else {
                    entity.lifecycle = EntityLifecycle::destroyed;
                    entity.destroyed_by_owner_index = player.player_index;
                }
                // PPC 0x3421C..0x34220 sets live +0xCA after successful 0x16300.
                // 0x36120 later uses it to suppress ordinary/group reward coins.
                entity.consumed_as_player_pickup = true;
                result.entity_became_inactive = true;
            }
            event.player_remained_active = player.status == 4;
            result.events.push_back(std::move(event));
            continue;
        }

        // 0x34228..0x342B8: passHitsToOwner redirects through a validated
        // parent safe-reference, otherwise the entity itself takes the global
        // Player_ImpactDamageToEntities amount.
        EntityRuntime* damage_target = first_damage_target(world, entity);
        event.entity_damage_target = damage_target->handle;
        const auto& target_definition = definition_or_throw(
            definition_for_unit, damage_target->unit_id);
        event.entity_damage = apply_collision_damage(
            *damage_target,
            target_definition,
            player_impact_damage_to_entities,
            player.player_index,
            current_tick,
            random,
            entity_hit_delay_ticks,
            removal_context,
            removal_trace);

        // 0x342BC..0x342F8 always calls player damage after the entity-side
        // leg, even if that leg destroyed the entity or its owner. The damage
        // amount is the colliding entity's UnitDef damage_FLOAT, not the
        // redirected target's damage value.
        event.player_damage_requested = entity.behavior.collision_damage;
        if (callbacks.apply_player_damage) {
            callbacks.apply_player_damage(
                player, event.player_damage_requested, current_tick);
        }
        event.player_remained_active = player.status == 4;
        ++result.reciprocal_impacts;
        result.events.push_back(std::move(event));
    }

    result.entity_became_inactive = result.entity_became_inactive || !active(entity);
    return result;
}

CollisionDamageResult apply_collision_damage(
    EntityRuntime& target,
    const UnitDefinition& target_definition,
    float damage,
    std::int8_t source_owner_index,
    std::uint32_t current_tick,
    LegacyRandom& random,
    int entity_hit_delay_ticks,
    LegacyRemovalContext* removal_context,
    LegacyRemovalTrace* removal_trace) {
    CollisionDamageResult result;
    if (!active(target)) return result;

    const auto tick = static_cast<std::int32_t>(current_tick);
    const auto hit_gate = wrapping_add(target.last_collision_hit_tick, entity_hit_delay_ticks);
    // PPC cmpw + ble: damage is accepted only on strict current > last+delay.
    if (tick <= hit_gate) return result;

    target.last_collision_hit_tick = tick;
    result.applied = true;
    result.shields_before = target.shields;

    const float depleted = static_cast<float>(target.shields - damage);
    target.shields = depleted < 0.0f ? 0.0f : depleted;
    result.absorbed_damage = static_cast<float>(result.shields_before - target.shields);
    result.shields_after = target.shields;

    // The original returns immediately if shields were already empty on entry.
    // last-hit bookkeeping above still occurred.
    if (result.shields_before <= 0.0f) return result;

    const auto& state_before = current_state(target);
    if (state_before.invulnerable_on_collision) {
        target.shields = result.shields_before;
        result.shields_after = target.shields;
        result.invulnerability_restored_shields = true;
    }

    if (state_before.hit_state_delay != 0 && !state_before.on_hit.original_label.empty()) {
        const auto transition_gate = wrapping_add(
            target.last_on_hit_transition_tick, state_before.hit_state_delay);
        if (tick > transition_gate) {
            target.last_on_hit_transition_tick = tick;
            result.on_hit_action_due = true;
            apply_entity_state_action(
                target, target_definition, state_before.on_hit, current_tick, random);
            if (!active(target)) {
                result.entity_destroyed = target.lifecycle == EntityLifecycle::destroyed;
                result.shields_after = target.shields;
                return result;
            }
        }
    }

    if (target.shields <= 0.0f) {
        result.score_award = target.behavior.score;

        // PPC 0x15060 tests live +0xCD after awarding score. The member
        // constructor 0x35DAC..0x35DF0 derives that byte by scanning every
        // state at compiled state +0x356, which the loader maps directly to
        // stateUseThisStateOnShieldDepletion_BOOL. When set, 0x17E70 enters
        // the first marked state in file order and skips ordinary 0x16300
        // destruction entirely.
        if (target.behavior.has_shield_depletion_state) {
            for (std::size_t i = 0; i < target.behavior.states.size(); ++i) {
                if (!target.behavior.states[i].use_on_shield_depletion) continue;
                enter_entity_state(target, target_definition, i, current_tick, random);
                result.shield_depletion_state_entered = true;
                result.shield_depletion_state_index = i;
                result.shields_after = target.shields;
                return result;
            }
            // A behavior compiled through compile_unit_behavior cannot reach
            // this inconsistency; preserve 0x17E70's no-op-on-no-match shape
            // rather than silently converting it into destruction.
            result.shields_after = target.shields;
            return result;
        }

        // Ordinary shield depletion enters 0x16300 immediately. Preserve
        // effect/RNG ordering whenever the removal context is available, with
        // a lifecycle-only fallback for bounded callers.
        if (removal_context && removal_trace) {
            (void)apply_legacy_destruction_effects(
                target, source_owner_index, *removal_context, random, *removal_trace);
        } else {
            target.lifecycle = EntityLifecycle::destroyed;
            target.destroyed_by_owner_index = source_owner_index;
        }
        result.entity_destroyed = true;
        result.shields_after = target.shields;
        return result;
    }

    // Non-lethal audiovisual side effects are surfaced as deterministic event
    // facts rather than performed by the headless core. The 1.0.6 routine
    // keeps the pre-hit compiled-state pointer in r30 across the optional
    // on-hit transition, so these fields deliberately come from state_before
    // even if apply_entity_state_action changed target.state.current_state.
    result.collision_glow_due = !state_before.do_not_glow_on_collision;
    result.hit_particles_due = !fourcc_is_none_or_empty(target.behavior.hit_particles);
    if (result.hit_particles_due) {
        const bool ground_space = target.behavior.collision_domain == kGroundDomain;
        result.hit_particle_spawn = make_legacy_particle_spawn_request(
            target.x, target.y, target.behavior.hit_particles,
            target.behavior.hit_particle_color, ground_space, 0);
        if (removal_context) {
            result.hit_particle_executed = execute_legacy_particle_spawn(
                removal_context->particle_execution, *result.hit_particle_spawn, random);
        }
    }

    if (!fourcc_is_none_or_empty(state_before.collision_spawn)) {
        const bool repetition_allows =
            state_before.collision_repeat_spawns || target.collision_spawn_count == 0;
        const auto spawn_gate = wrapping_add(
            target.last_collision_spawn_tick, state_before.collision_spawn_delay);
        // PPC 0x151A8 skips only when current < last+delay: equality is due.
        if (repetition_allows && tick >= spawn_gate) {
            result.collision_spawn_due = state_before.collision_spawn;
            target.last_collision_spawn_tick = tick;
            ++target.collision_spawn_count;
        }
    }

    result.shields_after = target.shields;
    return result;
}

bool legacy_collision_candidate_compatible(
    const EntityRuntime& self,
    const EntityRuntime& candidate) {
    if (!active(self) || !active(candidate)) return false;
    if (!current_state(self).collides || !current_state(candidate).collides) return false;
    if (!candidate.collision_participating) return false;
    if (candidate.serial == self.serial) return false;
    if (!(candidate.behavior.collision_domain == self.behavior.collision_domain)) return false;

    // 0x36E58..0x36E7C requires the classes to be opposite. A harmless entity
    // only scans non-harmless candidates; a non-harmless entity only scans
    // harmless candidates.
    if (candidate.behavior.harmless_to_players == self.behavior.harmless_to_players) return false;
    if (candidate.group_delay_ticks > 0) return false;

    if (self.behavior.player_projectile) {
        return candidate.behavior.can_be_hit_by_player_projectile;
    }
    return candidate.behavior.can_be_hit_by_player_projectile &&
           candidate.behavior.player_projectile;
}

CollisionPairResult apply_legacy_collision_pair(
    EntityWorld& world,
    EntityRuntime& self,
    EntityRuntime& candidate,
    std::uint32_t current_tick,
    const CollisionUnitDefinitionProvider& definition_for_unit,
    LegacyRandom& random,
    int entity_hit_delay_ticks,
    LegacyRemovalContext* removal_context,
    LegacyRemovalTrace* removal_trace) {
    CollisionPairResult result;
    result.collided = true;

    EntityRuntime* first_target = first_damage_target(world, self);
    result.first_damage_target = first_target->handle;
    result.first_damage_source_owner_index = candidate.player_owner_index;
    const auto& first_definition = definition_or_throw(definition_for_unit, first_target->unit_id);
    result.first_damage = apply_collision_damage(
        *first_target,
        first_definition,
        candidate.behavior.collision_damage,
        candidate.player_owner_index,
        current_tick,
        random,
        entity_hit_delay_ticks,
        removal_context,
        removal_trace);

    // The original re-evaluates the candidate's current state after the first
    // damage call, so a state transition in the first leg can affect the
    // second passHitsToOwner decision.
    EntityRuntime* second_target = second_damage_target(world, self, candidate);
    result.second_damage_target = second_target->handle;
    result.second_damage_source_owner_index = self.player_owner_index;
    const auto& second_definition = definition_or_throw(definition_for_unit, second_target->unit_id);
    result.second_damage = apply_collision_damage(
        *second_target,
        second_definition,
        self.behavior.collision_damage,
        self.player_owner_index,
        current_tick,
        random,
        entity_hit_delay_ticks,
        removal_context,
        removal_trace);

    return result;
}

CollisionScanResult scan_legacy_entity_collisions(
    EntityWorld& world,
    EntityRuntime& self,
    std::uint32_t current_tick,
    const CollisionUnitDefinitionProvider& definition_for_unit,
    LegacyRandom& random,
    int entity_hit_delay_ticks,
    LegacyRemovalContext* removal_context,
    LegacyRemovalTrace* removal_trace) {
    CollisionScanResult result;
    if (!active(self) || !current_state(self).collides) return result;

    const auto self_bounds = legacy_collision_bounds(self);
    if (self.behavior.player_projectile && self_bounds.max_y < 0) return result;

    // EntityWorld appends members in group/member order and does not reorder
    // them, which is the same traversal order as the original nested lists for
    // this bounded clean world model.
    for (auto& candidate : world.members()) {
        ++result.candidates_considered;
        if (!legacy_collision_candidate_compatible(self, candidate)) continue;

        const auto candidate_bounds = legacy_collision_bounds(candidate);
        if (candidate.behavior.player_projectile && candidate_bounds.max_y < 0) continue;
        if (!legacy_collision_bounds_overlap(self_bounds, candidate_bounds)) continue;
        ++result.aabb_overlaps;

        if (!legacy_radial_collision(self, self_bounds, candidate, candidate_bounds)) continue;
        ++result.radial_overlaps;

        auto pair = apply_legacy_collision_pair(
            world, self, candidate, current_tick, definition_for_unit, random,
            entity_hit_delay_ticks, removal_context, removal_trace);
        result.pairs.push_back(std::move(pair));
        ++result.collisions_applied;

        if (!active(self)) {
            result.self_became_inactive = true;
            break;
        }
    }
    return result;
}

} // namespace deimos
