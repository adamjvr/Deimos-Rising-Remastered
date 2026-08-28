#include "deimos/destruction_runtime.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <string>
#include <utility>

namespace {

deimos::FourCC id(const char (&text)[5]) {
    return deimos::FourCC{{text[0], text[1], text[2], text[3]}};
}

bool is_kind(
    const deimos::LegacyRemovalConsequence& event,
    deimos::LegacyRemovalConsequenceKind kind) {
    return event.kind == kind;
}

deimos::LegacyRandomBonusConfig bonus_config() {
    deimos::LegacyRandomBonusConfig c;
    c.percent_thresholds = {70, 78, 82, 84, 87, 91, 95, 98, 100};
    c.ground_accuracy_reward_percent = 10;
    c.minimum_progression_for_highest_bonus = 3;
    c.bonus_ids = {
        id("rb01"), id("rb02"), id("rb03"), id("rb04"), id("rb05"),
        id("rb06"), id("rb07"), id("rb08"), id("rb09"), id("rb10")
    };
    return c;
}

deimos::EntityRuntime entity(
    deimos::EntityHandle handle,
    std::uint32_t serial,
    std::uint32_t group_serial,
    deimos::FourCC unit_id) {
    deimos::EntityRuntime e;
    e.handle = handle;
    e.serial = serial;
    e.group_serial = group_serial;
    e.unit_id = unit_id;
    e.x = static_cast<float>(handle * 10);
    e.y = static_cast<float>(handle * 20);
    e.behavior.collision_domain = id("grnd");
    e.behavior.states.resize(1);
    e.state.current_state = 0;
    return e;
}

void add_group(
    deimos::EntityWorld& world,
    deimos::EntityRuntime member,
    deimos::FourCC group_unit_id = {}) {
    deimos::EntityGroupBuildResult build;
    build.status = deimos::EntityGroupBuildStatus::complete;
    deimos::EntityGroupRuntime group;
    group.serial = member.group_serial;
    group.unit_id = group_unit_id.bytes == std::array<char,4>{}
        ? member.unit_id : group_unit_id;
    group.member_count = 1;
    group.active_member_count = 1;
    group.destroyed_member_count = 0;
    build.group = group;
    build.members.push_back(std::move(member));
    world.register_group(std::move(build));
}

} // namespace

int main() {
    const auto config = bonus_config();

    // Resource binding is positional just like the executable, but checks the
    // canonical labels so the wrong list cannot silently satisfy the offsets.
    {
        deimos::NamedTable<float> floats(220);
        for (std::size_t i = 0; i < floats.size(); ++i) floats[i] = {"unused", 0.0f};
        const std::array<const char*, 11> names = {
            "Game_RandomBonusPercent_1", "Game_RandomBonusPercent_2",
            "Game_RandomBonusPercent_3", "Game_RandomBonusPercent_4",
            "Game_RandomBonusPercent_5", "Game_RandomBonusPercent_6",
            "Game_RandomBonusPercent_7", "Game_RandomBonusPercent_8",
            "Game_RandomBonusPercent_9", "Game_RandomBonusPercent_GroundAccuracyReward",
            "Game_MinimumLevelForHighestRandomBonus"
        };
        const std::array<float, 11> values = {
            70.9f, 78.0f, 82.0f, 84.0f, 87.0f, 91.0f, 95.0f, 98.0f, 100.0f, 10.0f, 3.0f
        };
        for (std::size_t i = 0; i < names.size(); ++i) floats[209 + i] = {names[i], values[i]};

        deimos::NamedTable<deimos::FourCC> objects(35);
        for (std::size_t i = 0; i < objects.size(); ++i) objects[i] = {"unused", {}};
        for (std::size_t i = 0; i < 10; ++i) {
            objects[25 + i] = {"RandomBonus_" + std::to_string(i + 1), config.bonus_ids[i]};
        }
        std::string error;
        const auto compiled = deimos::compile_legacy_random_bonus_config(floats, objects, &error);
        assert(compiled);
        assert(compiled->percent_thresholds[0] == 70); // fctiwz-style truncation
        assert(compiled->ground_accuracy_reward_percent == 10);
        assert(compiled->minimum_progression_for_highest_bonus == 3);
        assert(compiled->bonus_ids == config.bonus_ids);
        objects[25].first = "WrongLabel";
        assert(!deimos::compile_legacy_random_bonus_config(floats, objects, &error));
    }

    // 0x16538..0x167B8: inclusive roll, strict threshold comparisons, special
    // pending ground-accuracy reward and progression gate for the top tail.
    {
        deimos::LegacyRandomBonusContext ctx;
        ctx.progression_value = 3;
        ctx.ground_accuracy_reward_pending = true;
        auto selected = deimos::select_legacy_random_bonus(5, ctx, config);
        assert(selected.unit_id == id("rb06"));
        assert(selected.special_ground_accuracy_reward);
        assert(selected.consumed_ground_accuracy_reward);
        assert(!ctx.ground_accuracy_reward_pending);

        selected = deimos::select_legacy_random_bonus(69, ctx, config);
        assert(selected.unit_id == id("rb01"));
        selected = deimos::select_legacy_random_bonus(70, ctx, config);
        assert(selected.unit_id == id("rb02"));

        ctx.progression_value = 2;
        selected = deimos::select_legacy_random_bonus(98, ctx, config);
        assert(selected.unit_id == id("rb08"));
        ctx.progression_value = 3;
        selected = deimos::select_legacy_random_bonus(98, ctx, config);
        assert(selected.unit_id == id("rb09"));
        selected = deimos::select_legacy_random_bonus(100, ctx, config);
        assert(selected.unit_id == id("rb10"));
    }

    // 0x16300 consequence ordering: terrain -> particle -> spawn -> notice ->
    // sound -> random bonus. The random branch consumes exactly one 0..100 draw.
    {
        auto e = entity(1, 101, 1001, id("boom"));
        e.player_owner_index = 2;
        e.behavior.destruction_draw_to_terrain = true;
        e.behavior.destruction_particles = id("dust");
        e.behavior.destruction_particle_color = {1, 2, 3};
        e.behavior.destruction_spawn = id("frag");
        e.behavior.destruction_notice = "Destroyed";
        e.behavior.destruction_sound.id = id("kabm");
        e.behavior.destruction_sound.min_volume = 70;
        e.behavior.destruction_sound.max_volume = 90;
        e.behavior.destruction_sound.priority = 42;
        e.behavior.destruction_sound.min_pitch = 0.8f;
        e.behavior.destruction_sound.max_pitch = 1.2f;
        e.behavior.destruction_release_random_bonus = true;

        deimos::LegacyRemovalContext context;
        context.current_tick = 77;
        context.random_bonus.progression_value = 3;
        context.random_bonus_config = config;
        deimos::LegacyRandom random(12345);
        deimos::LegacyRandom expected_random(12345);
        const int expected_roll = deimos::choose_inclusive_integer(0, 100, expected_random);
        auto expected_context = context.random_bonus;
        const auto expected_bonus = deimos::select_legacy_random_bonus(
            expected_roll, expected_context, config);

        deimos::LegacyRemovalTrace trace;
        assert(deimos::apply_legacy_destruction_effects(
            e, 2, context, random, trace));
        assert(e.lifecycle == deimos::EntityLifecycle::destroyed);
        assert(e.destroyed_by_owner_index == 2);
        assert(e.destruction_effects_processed);
        assert(random.seed() == expected_random.seed());
        assert(trace.consequences.size() == 6);
        assert(is_kind(trace.consequences[0], deimos::LegacyRemovalConsequenceKind::terrain_draw));
        assert(is_kind(trace.consequences[1], deimos::LegacyRemovalConsequenceKind::destruction_particles));
        assert((trace.consequences[1].color == deimos::Rgb24{1, 2, 3}));
        assert(is_kind(trace.consequences[2], deimos::LegacyRemovalConsequenceKind::destruction_spawn));
        assert(trace.consequences[2].resource_id == id("frag"));
        assert(trace.consequences[2].spawn_request->parent.handle == e.handle);
        assert(trace.consequences[2].spawn_request->player_owner_index == 2);
        assert(is_kind(trace.consequences[3], deimos::LegacyRemovalConsequenceKind::destruction_notice));
        assert(trace.consequences[3].text == "Destroyed");
        assert(trace.consequences[3].tick == 77);
        assert(is_kind(trace.consequences[4], deimos::LegacyRemovalConsequenceKind::destruction_sound));
        assert(trace.consequences[4].sound.id == id("kabm"));
        assert(trace.consequences[4].sound.min_volume == 70);
        assert(trace.consequences[4].sound.max_volume == 90);
        assert(trace.consequences[4].sound.priority == 42);
        assert(trace.consequences[4].sound.min_pitch == 0.8f);
        assert(trace.consequences[4].sound.max_pitch == 1.2f);
        assert(is_kind(trace.consequences[5], deimos::LegacyRemovalConsequenceKind::random_bonus_spawn));
        assert(trace.consequences[5].resource_id == expected_bonus.unit_id);
        assert(!deimos::apply_legacy_destruction_effects(
            e, 2, context, random, trace));
    }

    // 0x36120 destroys willing children before its own group/reward work. A
    // player-attributed final member produces ordinary coins and one group-kill
    // coin; child destruction does not inherit player attribution implicitly.
    {
        deimos::EntityWorld world;
        auto parent = entity(10, 110, 1010, id("parn"));
        parent.destroyed_by_owner_index = 1;
        parent.behavior.destruction_destroy_children = true;
        parent.behavior.destruction_coin_count = 2;
        parent.behavior.destruction_coin = id("coin");
        parent.behavior.destruction_group_kill_coin = id("gkil");
        add_group(world, std::move(parent));

        auto child = entity(20, 220, 2020, id("chld"));
        child.parent = {10, 110};
        child.behavior.states[0].can_be_destroyed_on_owner_destruction = true;
        add_group(world, std::move(child));

        deimos::LegacyRemovalContext context;
        context.random_bonus_config = config;
        deimos::LegacyRandom random(1);
        deimos::LegacyRemovalTrace trace;
        auto* live_parent = world.find_member(10);
        auto* parent_group = world.find_group(1010);
        assert(live_parent && parent_group);
        assert(deimos::remove_legacy_group_member(
            world, *parent_group, *live_parent, true, true,
            context, random, trace));

        assert(world.find_member(20)->removal_processed);
        assert(world.find_member(20)->lifecycle == deimos::EntityLifecycle::destroyed);
        assert(world.find_group(2020)->destroyed_member_count == 1);
        assert(parent_group->destroyed_member_count == 1);
        assert(parent_group->active_member_count == 0);
        assert(trace.removals.size() == 2);
        assert(trace.removals[0].member == 20); // recursive child first
        assert(trace.removals[1].member == 10);
        assert(trace.removals[1].group_killed);
        assert(trace.removals[1].group_should_be_removed);

        int ordinary = 0;
        int group_kill = 0;
        for (const auto& event : trace.consequences) {
            if (event.kind == deimos::LegacyRemovalConsequenceKind::ordinary_coin_spawn) ++ordinary;
            if (event.kind == deimos::LegacyRemovalConsequenceKind::group_kill_coin_spawn) ++group_kill;
        }
        assert(ordinary == 2);
        assert(group_kill == 1);
    }

    // Successful pickups set +0xCA; 0x36120 must suppress both reward branches.
    {
        deimos::EntityWorld world;
        auto pickup = entity(30, 330, 3030, id("pick"));
        pickup.lifecycle = deimos::EntityLifecycle::destroyed;
        pickup.destroyed_by_owner_index = 0;
        pickup.consumed_as_player_pickup = true;
        pickup.behavior.destruction_coin_count = 4;
        pickup.behavior.destruction_coin = id("coin");
        pickup.behavior.destruction_group_kill_coin = id("gkil");
        add_group(world, std::move(pickup));

        deimos::LegacyRemovalContext context;
        context.random_bonus_config = config;
        deimos::LegacyRandom random(1);
        auto trace = deimos::finalize_legacy_pending_removals(world, context, random);
        for (const auto& event : trace.consequences) {
            assert(event.kind != deimos::LegacyRemovalConsequenceKind::ordinary_coin_spawn);
            assert(event.kind != deimos::LegacyRemovalConsequenceKind::group_kill_coin_spawn);
        }
    }

    // SERM is special: reaching destroyed==original still reports a group kill
    // internally, but emits no group-kill coin and active==0 does not request
    // removal of the group container.
    {
        deimos::EntityWorld world;
        auto e = entity(40, 440, 4040, id("xxxx"));
        e.destroyed_by_owner_index = 0;
        e.behavior.destruction_group_kill_coin = id("gkil");
        add_group(world, std::move(e), id("SERM"));
        deimos::LegacyRemovalContext context;
        context.random_bonus_config = config;
        deimos::LegacyRandom random(1);
        deimos::LegacyRemovalTrace trace;
        auto* member = world.find_member(40);
        auto* group = world.find_group(4040);
        assert(!deimos::remove_legacy_group_member(
            world, *group, *member, true, true, context, random, trace));
        assert(trace.removals.back().group_killed);
        assert(!trace.removals.back().group_should_be_removed);
        for (const auto& event : trace.consequences) {
            assert(event.kind != deimos::LegacyRemovalConsequenceKind::group_kill_coin_spawn);
        }
    }

    // 0x36610 outer pass: a destroyed child can destruct its still-active
    // owner via current-state +0x32D; a later-position owner is then finalized
    // in the same traversal. Deleted entities get deletionSpawn, and obstacle
    // conversion is independent of destruction-vs-deletion.
    {
        deimos::EntityWorld world;
        auto child = entity(50, 550, 5050, id("chd2"));
        child.lifecycle = deimos::EntityLifecycle::destroyed;
        child.destroyed_by_owner_index = 3;
        child.parent = {60, 660};
        child.behavior.states[0].destroy_owner_on_destruction = true;
        add_group(world, std::move(child));

        auto owner = entity(60, 660, 6060, id("ownr"));
        add_group(world, std::move(owner));

        auto deleted = entity(70, 770, 7070, id("dele"));
        deleted.lifecycle = deimos::EntityLifecycle::deleted;
        deleted.behavior.deletion_spawn = id("dels");
        deleted.behavior.destruction_create_obstacle = true;
        add_group(world, std::move(deleted));

        deimos::LegacyRemovalContext context;
        context.random_bonus_config = config;
        deimos::LegacyRandom random(1);
        const auto trace = deimos::finalize_legacy_pending_removals(world, context, random);
        assert(world.find_member(50)->removal_processed);
        assert(world.find_member(60)->removal_processed);
        assert(world.find_member(60)->lifecycle == deimos::EntityLifecycle::destroyed);
        assert(world.find_member(60)->destroyed_by_owner_index == 3);
        assert(world.find_member(70)->removal_processed);

        bool owner_trigger = false;
        bool deletion_spawn = false;
        bool obstacle = false;
        for (const auto& event : trace.consequences) {
            if (event.kind == deimos::LegacyRemovalConsequenceKind::owner_destruction_triggered &&
                event.source == 50 && event.related == 60) owner_trigger = true;
            if (event.kind == deimos::LegacyRemovalConsequenceKind::deletion_spawn &&
                event.source == 70 && event.resource_id == id("dels")) deletion_spawn = true;
            if (event.kind == deimos::LegacyRemovalConsequenceKind::obstacle_create &&
                event.source == 70) obstacle = true;
        }
        assert(owner_trigger);
        assert(deletion_spawn);
        assert(obstacle);
    }

    return 0;
}
