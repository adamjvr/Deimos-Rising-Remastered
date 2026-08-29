#include "deimos/destruction_runtime.hpp"

#include "deimos/spawn_runtime.hpp"
#include "deimos/state_runtime.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace deimos {
namespace {

constexpr FourCC fourcc(char a, char b, char c, char d) {
    return FourCC{{a, b, c, d}};
}

bool is_ground(const EntityRuntime& entity) {
    return entity.behavior.collision_domain == fourcc('g', 'r', 'n', 'd');
}

bool empty_or_none(FourCC id) {
    return id == FourCC{} || id.str() == "none" || id.str() == "NULL";
}

bool special_serm_group(const EntityGroupRuntime& group) {
    return group.unit_id == fourcc('S', 'E', 'R', 'M');
}

const CompiledUnitStateBehavior* current_state(const EntityRuntime& entity) {
    if (entity.state.current_state >= entity.behavior.states.size()) return nullptr;
    return &entity.behavior.states[entity.state.current_state];
}

SpawnRequestSeed consequence_spawn_seed(const EntityRuntime& source, FourCC unit_id) {
    SpawnRequestSeed request;
    request.unit_id = unit_id;
    request.x = source.x;
    request.y = source.y;
    request.player_owner_index = source.player_owner_index;
    request.parent = {source.handle, source.serial};
    return request;
}

void add_spawn(
    LegacyRemovalTrace& trace,
    LegacyRemovalConsequenceKind kind,
    const EntityRuntime& source,
    FourCC unit_id) {
    if (empty_or_none(unit_id)) return;
    LegacyRemovalConsequence event;
    event.kind = kind;
    event.source = source.handle;
    event.resource_id = unit_id;
    event.spawn_request = consequence_spawn_seed(source, unit_id);
    trace.consequences.push_back(std::move(event));
}

bool emit_legacy_removal_spawn(
    LegacyRemovalTrace& trace,
    LegacyRemovalConsequenceKind requested_kind,
    const EntityRuntime& source,
    FourCC requested_id,
    LegacyRemovalContext& context,
    LegacyRandom& random) {
    if (empty_or_none(requested_id)) return false;

    const auto media = resolve_legacy_removal_media(
        source,
        context.world_y_origin,
        context.water_impact_config,
        context.water_probe,
        random);

    if (media.replacement_spawn) {
        add_spawn(
            trace,
            LegacyRemovalConsequenceKind::water_impact_spawn,
            source,
            *media.replacement_spawn);
    }
    if (!media.allow_requested_spawn) return false;

    add_spawn(trace, requested_kind, source, requested_id);
    return true;
}


EntityGroupRuntime& require_group(EntityWorld& world, const EntityRuntime& member) {
    auto* group = world.find_group(member.group_serial);
    if (!group) throw std::runtime_error("entity removal references missing group serial");
    return *group;
}

std::vector<EntityHandle> willing_children(
    const EntityWorld& world,
    const EntityRuntime& owner,
    bool destruction) {
    std::vector<EntityHandle> result;
    for (const auto& candidate : world.members()) {
        if (candidate.handle == owner.handle || candidate.removal_processed) continue;
        // PPC 0x36474 / 0x3659C compares only parent serial +0x144 with the
        // owner's live-member serial +0x9C. It does not validate the pointer.
        if (candidate.parent.serial != owner.serial) continue;
        const auto* state = current_state(candidate);
        if (!state) continue;
        const bool willing = destruction
            ? state->can_be_destroyed_on_owner_destruction
            : state->can_be_deleted_on_owner_deletion;
        if (willing) result.push_back(candidate.handle);
    }
    return result;
}

} // namespace

std::optional<LegacyRandomBonusConfig> compile_legacy_random_bonus_config(
    const NamedTable<float>& game_floats,
    const NamedTable<FourCC>& game_objects,
    std::string* error) {
    constexpr std::size_t kFloatFirst = 209;
    constexpr std::size_t kFloatLast = 219;
    constexpr std::size_t kObjectFirst = 25;
    constexpr std::size_t kObjectLast = 34;
    if (game_floats.size() <= kFloatLast || game_objects.size() <= kObjectLast) {
        if (error) *error = "Game random-bonus tables are shorter than the 1.0.6 positional contract";
        return std::nullopt;
    }

    constexpr std::array<std::string_view, 11> float_names = {
        "Game_RandomBonusPercent_1",
        "Game_RandomBonusPercent_2",
        "Game_RandomBonusPercent_3",
        "Game_RandomBonusPercent_4",
        "Game_RandomBonusPercent_5",
        "Game_RandomBonusPercent_6",
        "Game_RandomBonusPercent_7",
        "Game_RandomBonusPercent_8",
        "Game_RandomBonusPercent_9",
        "Game_RandomBonusPercent_GroundAccuracyReward",
        "Game_MinimumLevelForHighestRandomBonus"
    };
    constexpr std::array<std::string_view, 10> object_names = {
        "RandomBonus_1", "RandomBonus_2", "RandomBonus_3", "RandomBonus_4", "RandomBonus_5",
        "RandomBonus_6", "RandomBonus_7", "RandomBonus_8", "RandomBonus_9", "RandomBonus_10"
    };

    for (std::size_t i = 0; i < float_names.size(); ++i) {
        if (game_floats[kFloatFirst + i].first != float_names[i]) {
            if (error) *error = "unexpected Game[gafl] random-bonus label at index " +
                std::to_string(kFloatFirst + i);
            return std::nullopt;
        }
    }
    for (std::size_t i = 0; i < object_names.size(); ++i) {
        if (game_objects[kObjectFirst + i].first != object_names[i]) {
            if (error) *error = "unexpected Objects[gaob] random-bonus label at index " +
                std::to_string(kObjectFirst + i);
            return std::nullopt;
        }
    }

    const auto trunc_i = [](float value) { return static_cast<int>(std::trunc(value)); };
    LegacyRandomBonusConfig out;
    for (std::size_t i = 0; i < out.percent_thresholds.size(); ++i) {
        out.percent_thresholds[i] = trunc_i(game_floats[kFloatFirst + i].second);
    }
    out.ground_accuracy_reward_percent = trunc_i(game_floats[218].second);
    out.minimum_progression_for_highest_bonus = trunc_i(game_floats[219].second);
    for (std::size_t i = 0; i < out.bonus_ids.size(); ++i) {
        out.bonus_ids[i] = game_objects[kObjectFirst + i].second;
    }
    return out;
}

LegacyRandomBonusSelection select_legacy_random_bonus(
    int roll,
    LegacyRandomBonusContext& context,
    const LegacyRandomBonusConfig& config) {
    LegacyRandomBonusSelection result;
    result.roll = roll;

    if (roll < config.percent_thresholds[0]) {
        if (context.ground_accuracy_reward_pending &&
            roll < config.ground_accuracy_reward_percent) {
            result.unit_id = config.bonus_ids[5]; // Game.gaob[30] / rb06 in 1.0.6
            result.special_ground_accuracy_reward = true;
            result.consumed_ground_accuracy_reward = true;
            context.ground_accuracy_reward_pending = false;
        } else {
            result.unit_id = config.bonus_ids[0];
        }
        return result;
    }

    for (std::size_t i = 1; i < 8; ++i) {
        if (roll < config.percent_thresholds[i]) {
            result.unit_id = config.bonus_ids[i];
            return result;
        }
    }

    // 0x16768..0x167A8: levels/progression below the configured minimum cannot
    // receive rb09/rb10; the tail collapses back to rb08.
    if (context.progression_value < config.minimum_progression_for_highest_bonus) {
        result.unit_id = config.bonus_ids[7];
        return result;
    }

    result.unit_id = roll < config.percent_thresholds[8]
        ? config.bonus_ids[8]
        : config.bonus_ids[9];
    return result;
}

bool apply_legacy_destruction_effects(
    EntityRuntime& entity,
    std::int8_t source_owner_index,
    LegacyRemovalContext& context,
    LegacyRandom& random,
    LegacyRemovalTrace& trace) {
    if (entity.destruction_effects_processed || entity.removal_processed) return false;

    // 0x16338..0x16368: only non-air entities draw their destruction into the
    // terrain, and only when UnitDef +0x4B3 is enabled.
    if (is_ground(entity) && entity.behavior.destruction_draw_to_terrain) {
        const auto rect = legacy_entity_world_rect(entity);
        if (context.ground_obstacles) context.ground_obstacles->add(rect);

        LegacyRemovalConsequence event;
        event.kind = LegacyRemovalConsequenceKind::terrain_draw;
        event.source = entity.handle;
        event.ground_based = true;
        event.rectangle = rect;
        trace.consequences.push_back(std::move(event));
    }

    if (!empty_or_none(entity.behavior.destruction_particles)) {
        LegacyRemovalConsequence event;
        event.kind = LegacyRemovalConsequenceKind::destruction_particles;
        event.source = entity.handle;
        event.resource_id = entity.behavior.destruction_particles;
        event.color = entity.behavior.destruction_particle_color;
        event.ground_based = is_ground(entity);
        event.particle_spawn = make_legacy_particle_spawn_request(
            entity.x, entity.y, entity.behavior.destruction_particles,
            entity.behavior.destruction_particle_color, event.ground_based, 0);
        event.particle_executed = execute_legacy_particle_spawn(
            context.particle_execution, *event.particle_spawn, random);
        trace.consequences.push_back(std::move(event));
    }

    if (!empty_or_none(entity.behavior.destruction_spawn)) {
        (void)emit_legacy_removal_spawn(
            trace,
            LegacyRemovalConsequenceKind::destruction_spawn,
            entity,
            entity.behavior.destruction_spawn,
            context,
            random);
    }

    if (!entity.behavior.destruction_notice.empty()) {
        LegacyRemovalConsequence event;
        event.kind = LegacyRemovalConsequenceKind::destruction_notice;
        event.source = entity.handle;
        event.text = entity.behavior.destruction_notice;
        event.tick = context.current_tick;
        trace.consequences.push_back(std::move(event));
    }

    if (!empty_or_none(entity.behavior.destruction_sound.id)) {
        LegacyRemovalConsequence event;
        event.kind = LegacyRemovalConsequenceKind::destruction_sound;
        event.source = entity.handle;
        event.resource_id = entity.behavior.destruction_sound.id;
        event.sound = entity.behavior.destruction_sound;
        trace.consequences.push_back(std::move(event));
    }

    // 0x164F8..0x16504 marks the live entity inactive/destructed before the
    // random-bonus branch. Preserve source attribution even for -1.
    entity.lifecycle = EntityLifecycle::destroyed;
    entity.destroyed_by_owner_index = source_owner_index;
    entity.destruction_effects_processed = true;

    if (entity.behavior.destruction_release_random_bonus) {
        const int roll = choose_inclusive_integer(0, 100, random);
        const auto selection = select_legacy_random_bonus(
            roll, context.random_bonus, context.random_bonus_config);
        if (!empty_or_none(selection.unit_id)) {
            add_spawn(
                trace, LegacyRemovalConsequenceKind::random_bonus_spawn,
                entity, selection.unit_id);
        }
    }

    return true;
}

bool remove_legacy_group_member(
    EntityWorld& world,
    EntityGroupRuntime& group,
    EntityRuntime& member,
    bool destruction,
    bool player_attributed,
    LegacyRemovalContext& context,
    LegacyRandom& random,
    LegacyRemovalTrace& trace) {
    if (member.removal_processed) return false;

    // 0x3614C..0x3618C: destroy willing children first, but only on a
    // destruction removal; deletion-willing children are then deleted for both
    // destruction and deletion of the owner.
    if (destruction && member.behavior.destruction_destroy_children) {
        for (const auto handle : willing_children(world, member, true)) {
            auto* child = world.find_member(handle);
            if (!child || child->removal_processed) continue;
            LegacyRemovalConsequence relation;
            relation.kind = LegacyRemovalConsequenceKind::child_destroy;
            relation.source = member.handle;
            relation.related = child->handle;
            trace.consequences.push_back(std::move(relation));
            auto& child_group = require_group(world, *child);
            const bool child_player_attributed = child->destroyed_by_owner_index != -1;
            (void)remove_legacy_group_member(
                world, child_group, *child, true, child_player_attributed,
                context, random, trace);
        }
    }

    if (member.behavior.destruction_delete_children) {
        for (const auto handle : willing_children(world, member, false)) {
            auto* child = world.find_member(handle);
            if (!child || child->removal_processed) continue;
            LegacyRemovalConsequence relation;
            relation.kind = LegacyRemovalConsequenceKind::child_delete;
            relation.source = member.handle;
            relation.related = child->handle;
            trace.consequences.push_back(std::move(relation));
            auto& child_group = require_group(world, *child);
            (void)remove_legacy_group_member(
                world, child_group, *child, false, false,
                context, random, trace);
        }
    }

    bool group_killed = false;
    if (destruction) {
        ++group.destroyed_member_count;
        group_killed = group.destroyed_member_count == group.member_count;
    }

    // 0x361B4..0x3634C: rewards require player-attributed destruction and are
    // suppressed for entities consumed through the pickup path (+0xCA).
    if (player_attributed && destruction && !member.consumed_as_player_pickup) {
        if (!empty_or_none(member.behavior.destruction_coin) &&
            member.behavior.destruction_coin_count > 0) {
            for (int i = 0; i < member.behavior.destruction_coin_count; ++i) {
                add_spawn(
                    trace, LegacyRemovalConsequenceKind::ordinary_coin_spawn,
                    member, member.behavior.destruction_coin);
            }
        }
        if (!special_serm_group(group) && group_killed &&
            !empty_or_none(member.behavior.destruction_group_kill_coin)) {
            add_spawn(
                trace, LegacyRemovalConsequenceKind::group_kill_coin_spawn,
                member, member.behavior.destruction_group_kill_coin);
        }
    }

    if (destruction) {
        (void)apply_legacy_destruction_effects(
            member, member.destroyed_by_owner_index,
            context, random, trace);
    } else {
        member.lifecycle = EntityLifecycle::deleted;
    }

    member.removal_processed = true;
    if (group.active_member_count > 0) --group.active_member_count;

    const bool group_should_be_removed =
        group.active_member_count <= 0 && !special_serm_group(group);

    LegacyRemovalConsequence removed;
    removed.kind = LegacyRemovalConsequenceKind::member_removed;
    removed.source = member.handle;
    trace.consequences.push_back(std::move(removed));

    trace.removals.push_back({
        member.handle,
        group.serial,
        destruction,
        player_attributed,
        group_killed,
        group_should_be_removed
    });
    return group_should_be_removed;
}

LegacyRemovalTrace finalize_legacy_pending_removals(
    EntityWorld& world,
    LegacyRemovalContext& context,
    LegacyRandom& random) {
    LegacyRemovalTrace trace;

    // Keep the original traversal property: this is a single forward pass over
    // the world insertion order. Recursive child removals mark their records so
    // a later encounter in this pass becomes a no-op.
    for (std::size_t i = 0; i < world.members().size(); ++i) {
        auto& member = world.members()[i];
        if (member.lifecycle == EntityLifecycle::active || member.removal_processed) continue;

        const bool destruction = member.lifecycle == EntityLifecycle::destroyed;

        // 0x366E4..0x36710: obstacle conversion is outside 0x36120 and runs for
        // either kind of inactive member when UnitDef +0x4B2 requests it.
        if (member.behavior.destruction_create_obstacle) {
            LegacyRemovalConsequence event;
            event.kind = LegacyRemovalConsequenceKind::obstacle_create;
            event.source = member.handle;
            event.rectangle = legacy_entity_world_rect(member);
            event.casts_shadows = member.behavior.casts_shadows;
            trace.consequences.push_back(std::move(event));
        }

        if (destruction) {
            // 0x36714..0x36770: current-state +0x32D may destruct the validated
            // owner with the child's source-owner byte. resolve_reference also
            // enforces the original serial + active checks.
            const auto* state = current_state(member);
            if (state && state->destroy_owner_on_destruction) {
                if (auto* owner = world.resolve_reference(member.parent)) {
                    LegacyRemovalConsequence event;
                    event.kind = LegacyRemovalConsequenceKind::owner_destruction_triggered;
                    event.source = member.handle;
                    event.related = owner->handle;
                    trace.consequences.push_back(std::move(event));
                    (void)apply_legacy_destruction_effects(
                        *owner, member.destroyed_by_owner_index,
                        context, random, trace);
                }
            }
        } else if (!empty_or_none(member.behavior.deletion_spawn)) {
            (void)emit_legacy_removal_spawn(
                trace,
                LegacyRemovalConsequenceKind::deletion_spawn,
                member,
                member.behavior.deletion_spawn,
                context,
                random);
        }

        auto& group = require_group(world, member);
        const bool player_attributed = member.destroyed_by_owner_index != -1;
        (void)remove_legacy_group_member(
            world, group, member, destruction, player_attributed,
            context, random, trace);
    }

    return trace;
}

} // namespace deimos
