#include "deimos/entity_world.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace deimos {
namespace {

bool is_active(const EntityRuntime& entity) {
    return entity.lifecycle == EntityLifecycle::active;
}

bool state_bool(
    const UnitDefinition& unit,
    const EntityRuntime& entity,
    std::string_view key) {
    if (entity.state.current_state >= unit.states.size()) {
        throw std::out_of_range("owner-location current state outside Unit Definition");
    }
    return unit.states[entity.state.current_state].fields.bool_value(key).value_or(false);
}

float original_orbit_radius(EntityPoint owner, EntityPoint self) {
    // PPC 0x42E90 first converts squared distance to a signed integer, then
    // evaluates sqrt and the caller truncates the resulting float again before
    // storing it back as a float at live +0xDC. Preserve those integer gates.
    const float dx = static_cast<float>(self.x - owner.x);
    const float dy = static_cast<float>(self.y - owner.y);
    const float squared = std::fma(dx, dx, static_cast<float>(dy * dy));
    const int squared_integer = static_cast<int>(std::trunc(squared));
    if (squared_integer <= 0) return 0.0f;
    const float distance = static_cast<float>(std::sqrt(static_cast<double>(squared_integer)));
    return static_cast<float>(static_cast<int>(std::trunc(distance)));
}

int wrap_heading(int heading) {
    if (heading > 359) heading -= 360;
    else if (heading < 0) heading += 360;
    return heading;
}

} // namespace

void EntityWorld::register_group(EntityGroupBuildResult&& build) {
    if (!build.constructed() || !build.group) {
        throw std::invalid_argument("cannot register an unconstructed entity group");
    }
    groups_.push_back(std::move(*build.group));
    members_.reserve(members_.size() + build.members.size());
    for (auto& member : build.members) members_.push_back(std::move(member));
}

EntityRuntime* EntityWorld::find_member(EntityHandle handle) {
    if (handle == kNoEntityHandle) return nullptr;
    auto it = std::find_if(members_.begin(), members_.end(), [handle](const auto& member) {
        return member.handle == handle;
    });
    return it == members_.end() ? nullptr : &*it;
}

const EntityRuntime* EntityWorld::find_member(EntityHandle handle) const {
    if (handle == kNoEntityHandle) return nullptr;
    auto it = std::find_if(members_.begin(), members_.end(), [handle](const auto& member) {
        return member.handle == handle;
    });
    return it == members_.end() ? nullptr : &*it;
}

EntityRuntime* EntityWorld::resolve_reference(const EntityReference& reference) {
    auto* member = find_member(reference.handle);
    if (!member || !is_active(*member) || member->serial != reference.serial) return nullptr;
    return member;
}

const EntityRuntime* EntityWorld::resolve_reference(const EntityReference& reference) const {
    const auto* member = find_member(reference.handle);
    if (!member || !is_active(*member) || member->serial != reference.serial) return nullptr;
    return member;
}

EntityRuntime* EntityWorld::find_first_active_unit(FourCC unit_id) {
    auto it = std::find_if(members_.begin(), members_.end(), [unit_id](const auto& member) {
        return is_active(member) && member.unit_id == unit_id;
    });
    return it == members_.end() ? nullptr : &*it;
}

const EntityRuntime* EntityWorld::find_first_active_unit(FourCC unit_id) const {
    auto it = std::find_if(members_.begin(), members_.end(), [unit_id](const auto& member) {
        return is_active(member) && member.unit_id == unit_id;
    });
    return it == members_.end() ? nullptr : &*it;
}

bool EntityWorld::has_active_unit(FourCC unit_id) const {
    return find_first_active_unit(unit_id) != nullptr;
}

std::size_t EntityWorld::active_member_count() const {
    return static_cast<std::size_t>(std::count_if(members_.begin(), members_.end(), [](const auto& member) {
        return is_active(member);
    }));
}

std::size_t EntityWorld::mark_owned_unit_deleted(
    FourCC unit_id,
    std::int8_t player_owner_index) {
    std::size_t count = 0;
    for (auto& member : members_) {
        if (!is_active(member)) continue;
        if (member.unit_id != unit_id) continue;
        if (member.player_owner_index != player_owner_index) continue;
        member.lifecycle = EntityLifecycle::deleted;
        ++count;
    }
    return count;
}

std::optional<EntityPoint> resolve_entity_owner_position(
    const EntityWorld& world,
    const EntityRuntime& entity,
    const PlayerPositionProvider& player_position) {
    if (!entity.parent.empty()) {
        if (const auto* parent = world.resolve_reference(entity.parent)) {
            return EntityPoint{parent->x, parent->y};
        }
    }
    if (entity.player_owner_index != -1 && player_position) {
        return player_position(entity.player_owner_index);
    }
    return std::nullopt;
}

EntityOwnerLocationMode current_owner_location_mode(
    const UnitDefinition& unit,
    const EntityRuntime& entity) {
    // Canonical 1.0.6 states use these modes mutually exclusively. The main
    // update checks them in Lock -> Link -> Orbit order; preserve that order
    // if non-canonical content ever sets multiple bits.
    if (state_bool(unit, entity, "stateLockToOwnerLoc_BOOL")) {
        return EntityOwnerLocationMode::lock_to_owner_location;
    }
    if (state_bool(unit, entity, "stateLinkToOwnerLoc_BOOL")) {
        return EntityOwnerLocationMode::link_to_owner_location;
    }
    if (state_bool(unit, entity, "stateOrbitOwner_BOOL")) {
        return EntityOwnerLocationMode::orbit_owner;
    }
    return EntityOwnerLocationMode::none;
}

bool initialize_entity_owner_location(
    EntityRuntime& entity,
    const UnitDefinition& unit,
    const std::optional<EntityPoint>& owner_position) {
    entity.owner_offset_x = 0.0f;
    entity.owner_offset_y = 0.0f;
    entity.previous_owner_x = 0.0f;
    entity.previous_owner_y = 0.0f;
    entity.orbit_radius = 0.0f;
    entity.orbit_angle_degrees = 0;
    entity.owner_location_initialized_state = entity.state.current_state;

    const auto mode = current_owner_location_mode(unit, entity);
    if (mode == EntityOwnerLocationMode::none || !owner_position) return false;

    entity.previous_owner_x = owner_position->x;
    entity.previous_owner_y = owner_position->y;

    if (mode == EntityOwnerLocationMode::orbit_owner ||
        mode == EntityOwnerLocationMode::lock_to_owner_location) {
        entity.owner_offset_x = static_cast<float>(entity.x - owner_position->x);
        entity.owner_offset_y = static_cast<float>(entity.y - owner_position->y);
    }

    if (mode == EntityOwnerLocationMode::orbit_owner) {
        entity.orbit_radius = original_orbit_radius(*owner_position, {entity.x, entity.y});
        entity.orbit_angle_degrees = legacy_angle_between_integer_points(
            static_cast<int>(std::trunc(owner_position->x)),
            static_cast<int>(std::trunc(owner_position->y)),
            static_cast<int>(std::trunc(entity.x)),
            static_cast<int>(std::trunc(entity.y)));
    }
    return true;
}

bool advance_entity_owner_location(
    EntityRuntime& entity,
    const UnitDefinition& unit,
    const std::optional<EntityPoint>& owner_position,
    const LegacyTrigTables& trig) {
    if (entity.lifecycle != EntityLifecycle::active) return false;

    // 0x33600 always clears/initializes its bookkeeping on state entry even
    // when no parent/player position can be resolved. Preserve that distinction:
    // if an owner appears later, Link observes the original zero history rather
    // than retroactively initializing to the new owner position.
    if (!entity.owner_location_initialized_state ||
        *entity.owner_location_initialized_state != entity.state.current_state) {
        (void)initialize_entity_owner_location(entity, unit, owner_position);
    }
    if (!owner_position) return false;

    if (entity.state.current_state >= unit.states.size()) {
        throw std::out_of_range("owner-location current state outside Unit Definition");
    }
    const auto& fields = unit.states[entity.state.current_state].fields;
    const bool lock = fields.bool_value("stateLockToOwnerLoc_BOOL").value_or(false);
    const bool link = fields.bool_value("stateLinkToOwnerLoc_BOOL").value_or(false);
    const bool orbit = fields.bool_value("stateOrbitOwner_BOOL").value_or(false);
    bool changed = false;

    // Main update 0x3401C..0x34054 executes these as three independent tests in
    // Lock -> Link -> Orbit order. Canonical 1.0.6 happens to use the bits
    // mutually exclusively, but retain the executable's ordering for external
    // data/mod compatibility.
    if (lock) {
        const float target_x = static_cast<float>(owner_position->x + entity.owner_offset_x);
        const float target_y = static_cast<float>(owner_position->y + entity.owner_offset_y);
        if (entity.x != target_x || entity.y != target_y) {
            entity.x = target_x;
            entity.y = target_y;
            changed = true;
        }
    }

    if (link) {
        const float owner_delta_x = static_cast<float>(owner_position->x - entity.previous_owner_x);
        const float owner_delta_y = static_cast<float>(owner_position->y - entity.previous_owner_y);
        entity.x = static_cast<float>(entity.x + owner_delta_x);
        entity.y = static_cast<float>(entity.y + owner_delta_y);
        entity.previous_owner_x = owner_position->x;
        entity.previous_owner_y = owner_position->y;
        changed = changed || owner_delta_x != 0.0f || owner_delta_y != 0.0f;
    }

    if (orbit) {
        // 0x373FC exits Orbit immediately when owner and member positions are
        // identical. Earlier Lock/Link work (for non-canonical mixed flags)
        // therefore participates in this exact check.
        if (entity.x != owner_position->x || entity.y != owner_position->y) {
            if (entity.orbit_radius != 0.0f) {
                // PPC 0x3743C truncates live +0x10. 0x37B50 proves +0x10/+0x14
                // are velocity X/Y, so Orbit intentionally repurposes integer VX
                // as its angle step when non-zero.
                const int angle_step = static_cast<int>(std::trunc(entity.velocity_x));
                if (angle_step != 0) {
                    entity.orbit_angle_degrees = wrap_heading(
                        entity.orbit_angle_degrees + angle_step);
                    const float vx = static_cast<float>(
                        entity.orbit_radius * trig.sine(entity.orbit_angle_degrees));
                    const float vy = static_cast<float>(
                        entity.orbit_radius * trig.cosine(entity.orbit_angle_degrees));
                    entity.x = static_cast<float>(owner_position->x + vx);
                    entity.y = static_cast<float>(owner_position->y + vy);
                } else {
                    entity.x = static_cast<float>(owner_position->x + entity.owner_offset_x);
                    entity.y = static_cast<float>(owner_position->y + entity.owner_offset_y);
                }
            } else {
                entity.x = static_cast<float>(owner_position->x + entity.owner_offset_x);
                entity.y = static_cast<float>(owner_position->y + entity.owner_offset_y);
            }

            entity.owner_offset_x = static_cast<float>(entity.x - owner_position->x);
            entity.owner_offset_y = static_cast<float>(entity.y - owner_position->y);
            changed = true;
        }
    }

    return changed;
}

bool advance_entity_owner_location_from_world(
    EntityWorld& world,
    EntityRuntime& entity,
    const UnitDefinition& unit,
    const PlayerPositionProvider& player_position,
    const LegacyTrigTables& trig) {
    const auto owner = resolve_entity_owner_position(world, entity, player_position);
    return advance_entity_owner_location(entity, unit, owner, trig);
}

EntityTickResult advance_entity_runtime_in_world(
    EntityWorld& world,
    EntityRuntime& entity,
    const UnitDefinition& unit,
    const EntityTickContext& context,
    const PlayerPositionProvider& player_position,
    LegacyRandom& random,
    const LegacyTrigTables& trig) {
    auto world_context = context;
    world_context.owner_location_phase = [&](EntityRuntime& live) {
        (void)advance_entity_owner_location_from_world(
            world, live, unit, player_position, trig);
    };
    return advance_entity_runtime(entity, unit, world_context, random);
}

} // namespace deimos
