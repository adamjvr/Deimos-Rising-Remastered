#include "deimos/collision_runtime.hpp"
#include "deimos/destruction_runtime.hpp"
#include "deimos/player_runtime.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <map>
#include <string>

namespace {

deimos::FourCC id(const char (&text)[5]) {
    return deimos::FourCC{{text[0], text[1], text[2], text[3]}};
}

deimos::DefinitionField f_bool(const char* key, bool value) {
    return {key, value, value ? "TRUE" : "FALSE", 0};
}
deimos::DefinitionField f_int(const char* key, int value) {
    return {key, value, std::to_string(value), 0};
}
deimos::DefinitionField f_float(const char* key, float value) {
    return {key, value, std::to_string(value), 0};
}
deimos::DefinitionField f_string(const char* key, const char* value) {
    return {key, std::string(value), value, 0};
}
deimos::DefinitionField f_id(const char* key, deimos::FourCC value) {
    return {key, value, value.str(), 0};
}
deimos::DefinitionField f_color(const char* key, deimos::Rgb24 value) {
    return {key, value, std::to_string(value.red) + "," +
        std::to_string(value.green) + "," + std::to_string(value.blue), 0};
}

struct UnitOptions {
    bool ground = false;
    bool harmless = false;
    bool projectile = false;
    bool hittable = true;
    float damage = 1.0f;
    float shields = 10.0f;
    int score = 25;
    bool collides = true;
    bool collides_players = false;
    bool pass_hits = false;
    bool invulnerable = false;
    bool no_glow = false;
    int hit_delay = 0;
    const char* on_hit = "";
    deimos::FourCC collision_spawn = id("none");
    bool collision_repeat = false;
    int collision_spawn_delay = 0;
    deimos::FourCC pickup_type = id("none");
    int pickup_value = 0;
};

deimos::UnitDefinition make_unit(const UnitOptions& o) {
    deimos::UnitDefinition u;
    u.name = "Collision test";
    u.family_name = "Test";
    u.description = "";
    u.core_fields.add(f_bool("isGroundBased_BOOL", o.ground));
    u.core_fields.add(f_bool("harmlessToPlayers_BOOL", o.harmless));
    u.core_fields.add(f_bool("playerProjectile_BOOL", o.projectile));
    u.core_fields.add(f_bool("canBeHitByPlayerProjectile_BOOL", o.hittable));
    u.core_fields.add(f_bool("hittableWhenInvisible_BOOL", false));
    u.core_fields.add(f_float("damage_FLOAT", o.damage));
    u.core_fields.add(f_float("shields_BaseAmount_FLOAT", o.shields));
    u.core_fields.add(f_float("shields_LevelIncrement_FLOAT", 0.0f));
    u.core_fields.add(f_float("shields_MaxAmount_FLOAT", o.shields));
    u.core_fields.add(f_id("hitParticles_ID", id("tiny")));
    u.core_fields.add(f_bool("hitParticleDoCircularBurst_BOOL", false));
    u.core_fields.add(f_color("hitParticlesColor_COLOR", {248, 128, 64}));
    u.core_fields.add(f_int("score_INT", o.score));
    u.core_fields.add(f_id("pickup_Type_ID", o.pickup_type));
    u.core_fields.add(f_int("pickup_Value_INT", o.pickup_value));

    deimos::UnitStateDefinition s0;
    s0.name = "Initial";
    s0.fields.add(f_bool("passHitsToOwner_BOOL", o.pass_hits));
    s0.fields.add(f_bool("stateCollides_BOOL", o.collides));
    s0.fields.add(f_bool("stateInvulnerable_ShieldsDoNotDepleteOnCollision_BOOL", o.invulnerable));
    s0.fields.add(f_bool("stateCollidesWithPlayers_BOOL", o.collides_players));
    s0.fields.add(f_bool("stateDoNotGlowOnCollision_BOOL", o.no_glow));
    s0.fields.add(f_id("collision_Spawn_ID", o.collision_spawn));
    s0.fields.add(f_bool("collision_RepeatSpawns_BOOL", o.collision_repeat));
    s0.fields.add(f_int("collision_SpawnDelay_INT", o.collision_spawn_delay));
    s0.fields.add(f_string("stateOnHitChangeTo_STR", o.on_hit));
    s0.fields.add(f_int("stateOnHitChangeStateDelay_INT", o.hit_delay));
    s0.fields.add(f_float("stateOnRange_FLOAT", 0.0f));
    s0.fields.add(f_string("stateOnRangeChangeTo_STR", ""));
    s0.fields.add(f_int("stateOnTimerMin_INT", 0));
    s0.fields.add(f_int("stateOnTimerMax_INT", 0));
    s0.fields.add(f_string("stateOnTimerChangeTo_STR", ""));
    s0.fields.add(f_int("stateOnCounter_INT", 0));
    s0.fields.add(f_string("stateOnCounterChangeTo_STR", ""));
    u.states.push_back(std::move(s0));

    deimos::UnitStateDefinition s1;
    s1.name = "Hit";
    s1.fields.add(f_bool("passHitsToOwner_BOOL", false));
    s1.fields.add(f_bool("stateCollides_BOOL", true));
    s1.fields.add(f_bool("stateInvulnerable_ShieldsDoNotDepleteOnCollision_BOOL", false));
    s1.fields.add(f_bool("stateCollidesWithPlayers_BOOL", false));
    s1.fields.add(f_bool("stateDoNotGlowOnCollision_BOOL", false));
    s1.fields.add(f_id("collision_Spawn_ID", id("none")));
    s1.fields.add(f_bool("collision_RepeatSpawns_BOOL", false));
    s1.fields.add(f_int("collision_SpawnDelay_INT", 0));
    s1.fields.add(f_string("stateOnHitChangeTo_STR", ""));
    s1.fields.add(f_int("stateOnHitChangeStateDelay_INT", 0));
    s1.fields.add(f_float("stateOnRange_FLOAT", 0.0f));
    s1.fields.add(f_string("stateOnRangeChangeTo_STR", ""));
    s1.fields.add(f_int("stateOnTimerMin_INT", 0));
    s1.fields.add(f_int("stateOnTimerMax_INT", 0));
    s1.fields.add(f_string("stateOnTimerChangeTo_STR", ""));
    s1.fields.add(f_int("stateOnCounter_INT", 0));
    s1.fields.add(f_string("stateOnCounterChangeTo_STR", ""));
    u.states.push_back(std::move(s1));
    return u;
}

deimos::EntityRuntime make_entity(
    deimos::EntityHandle handle,
    std::uint32_t serial,
    deimos::FourCC unit_id,
    const deimos::UnitDefinition& unit) {
    deimos::EntityRuntime e;
    e.handle = handle;
    e.serial = serial;
    e.unit_id = unit_id;
    e.behavior = deimos::compile_unit_behavior(unit);
    e.state.current_state = 0;
    e.shields = e.behavior.shields_base;
    e.collision_half_width = 5;
    e.collision_half_height = 4;
    e.collision_participating = true;
    return e;
}

bool near(float a, float b) {
    return std::fabs(a - b) < 1.0e-5f;
}

} // namespace

int main() {
    // 0x12AD0: edges truncate toward zero independently; touching AABBs are
    // retained for the radial second stage.
    auto geom_unit = make_unit(UnitOptions{});
    auto geom = make_entity(1, 1, id("geom"), geom_unit);
    geom.x = -0.75f;
    geom.y = 10.9f;
    geom.collision_half_width = 2;
    geom.collision_half_height = 3;
    const auto bounds = deimos::legacy_collision_bounds(geom);
    assert(bounds.min_x == -2);
    assert(bounds.max_x == 1);
    assert(bounds.min_y == 7);
    assert(bounds.max_y == 13);
    assert(deimos::legacy_collision_bounds_overlap(bounds, {1, 13, 4, 16}));
    assert(!deimos::legacy_collision_bounds_overlap(bounds, {2, 14, 4, 16}));

    // 0x36F60..0x36FB8 + 0x42F80: radius is half the already-truncated
    // vertical AABB span, center distance truncates its squared value before
    // sqrt, and equality with the radius sum is NOT a collision.
    assert(deimos::legacy_collision_radius(bounds) == 3);
    assert(near(deimos::legacy_quantized_center_distance(0.0f, 0.0f, 3.9f, 0.0f),
                std::sqrt(15.0f)));
    auto radial_a = make_entity(2, 2, id("ra01"), geom_unit);
    auto radial_b = make_entity(3, 3, id("ra02"), geom_unit);
    radial_a.x = 0.0f;
    radial_a.y = 0.0f;
    radial_b.x = 8.0f;
    radial_b.y = 0.0f;
    auto radial_a_bounds = deimos::legacy_collision_bounds(radial_a);
    auto radial_b_bounds = deimos::legacy_collision_bounds(radial_b);
    // Radii are 4+4 and center distance is exactly 8: strict LT rejects.
    assert(deimos::legacy_collision_bounds_overlap(radial_a_bounds, radial_b_bounds));
    assert(!deimos::legacy_radial_collision(
        radial_a, radial_a_bounds, radial_b, radial_b_bounds));
    radial_b.x = 7.9f;
    radial_b_bounds = deimos::legacy_collision_bounds(radial_b);
    assert(deimos::legacy_radial_collision(
        radial_a, radial_a_bounds, radial_b, radial_b_bounds));
    // A corner can survive AABB rejection but fail the radial second stage.
    radial_b.x = 8.0f;
    radial_b.y = 6.0f;
    radial_b_bounds = deimos::legacy_collision_bounds(radial_b);
    assert(deimos::legacy_collision_bounds_overlap(radial_a_bounds, radial_b_bounds));
    assert(!deimos::legacy_radial_collision(
        radial_a, radial_a_bounds, radial_b, radial_b_bounds));

    // Player geometry uses the same Rect truncation but a float 0.5*height
    // radius. A fractional center can therefore produce a half-integer player
    // radius while the entity side still truncates its span/2 to integer.
    deimos::PlayerRuntimeSlot player_geom;
    player_geom.status = 4;
    player_geom.x = 0.0f;
    player_geom.y = 0.5f;
    player_geom.collision_half_width = 5;
    player_geom.collision_half_height = 4;
    const auto player_bounds = deimos::legacy_player_collision_bounds(player_geom);
    assert(player_bounds.min_y == -3 && player_bounds.max_y == 4);
    assert(near(deimos::legacy_player_collision_radius(player_bounds), 3.5f));
    auto entity_geom = make_entity(4, 4, id("ep01"), geom_unit);
    entity_geom.x = 6.0f;
    entity_geom.y = 0.5f;
    const auto entity_bounds = deimos::legacy_collision_bounds(entity_geom);
    assert(deimos::legacy_collision_radius(entity_bounds) == 3);
    assert(deimos::legacy_entity_player_geometry_overlap(
        entity_geom, entity_bounds, player_geom, player_bounds));
    entity_geom.x = 6.5f;
    const auto equality_bounds = deimos::legacy_collision_bounds(entity_geom);
    // Raw geometric equality is still a hit here: 6.5^2 truncates to 42,
    // sqrt(42) < 6.5. This is an observable consequence of 0x42F80's fctiwz.
    assert(deimos::legacy_entity_player_geometry_overlap(
        entity_geom, equality_bounds, player_geom, player_bounds));
    entity_geom.x = 6.56f;
    const auto beyond_bounds = deimos::legacy_collision_bounds(entity_geom);
    assert(!deimos::legacy_entity_player_geometry_overlap(
        entity_geom, beyond_bounds, player_geom, player_bounds));

    // Main-tick viewport guard: left permits exactly 32 pixels offscreen;
    // top permits no negative-only bounds. Right/bottom limits are inclusive.
    assert(deimos::legacy_entity_within_player_collision_viewport(
        {-40, 0, -32, 8}, {320, 240}));
    assert(!deimos::legacy_entity_within_player_collision_viewport(
        {-41, 0, -33, 8}, {320, 240}));
    assert(deimos::legacy_entity_within_player_collision_viewport(
        {320, 240, 328, 248}, {320, 240}));
    assert(!deimos::legacy_entity_within_player_collision_viewport(
        {321, 0, 329, 8}, {320, 240}));
    assert(!deimos::legacy_entity_within_player_collision_viewport(
        {0, 241, 8, 249}, {320, 240}));

    // 0x34090..0x34314 ordinary player impacts. Two active overlapping players
    // are snapshotted before the loop. The first damage callback deactivates
    // player 0, but player 1 still receives the second reciprocal impact. The
    // entity's second same-tick damage is suppressed by Entity_HitDelay.
    UnitOptions player_impact_o;
    player_impact_o.collides_players = true;
    player_impact_o.shields = 300.0f;
    player_impact_o.damage = 7.0f;
    auto player_impact_u = make_unit(player_impact_o);
    deimos::EntityWorld player_world_entities;
    player_world_entities.members().push_back(
        make_entity(40, 40, id("pimp"), player_impact_u));
    auto* impact_entity = player_world_entities.find_member(40);
    impact_entity->x = 100.0f;
    impact_entity->y = 100.0f;

    deimos::PlayerWorld impact_players;
    for (std::size_t i = 0; i < impact_players.slots().size(); ++i) {
        auto& p = impact_players.slots()[i];
        p.status = 4;
        p.x = 100.0f;
        p.y = 100.0f;
        p.player_index = static_cast<std::int8_t>(i);
        p.collision_half_width = 5;
        p.collision_half_height = 4;
    }
    std::map<std::string, const deimos::UnitDefinition*> player_defs = {
        {"pimp", &player_impact_u}
    };
    const auto player_provider = [&](deimos::FourCC unit_id) -> const deimos::UnitDefinition* {
        auto it = player_defs.find(unit_id.str());
        return it == player_defs.end() ? nullptr : it->second;
    };
    deimos::LegacyRandom rng(1);
    int player_damage_calls = 0;
    deimos::LegacyPlayerCollisionCallbacks impact_callbacks;
    impact_callbacks.apply_player_damage = [&](
        deimos::PlayerRuntimeSlot& p, float damage, std::uint32_t tick) {
        assert(near(damage, 7.0f));
        assert(tick == 2);
        ++player_damage_calls;
        if (p.player_index == 0) p.status = 5;
    };
    auto player_scan = deimos::scan_legacy_player_collisions(
        player_world_entities, *impact_entity, impact_players, {320, 240}, 2,
        player_provider, rng, impact_callbacks, 100.0f, 1);
    assert(player_scan.players_considered == 2);
    assert(player_scan.aabb_overlaps == 2);
    assert(player_scan.radial_overlaps == 2);
    assert(player_scan.reciprocal_impacts == 2);
    assert(player_scan.events.size() == 2);
    assert(player_scan.events[0].entity_damage.applied);
    assert(!player_scan.events[1].entity_damage.applied);
    assert(!player_scan.events[0].player_remained_active);
    assert(player_scan.events[1].player_remained_active);
    assert(player_damage_calls == 2);
    assert(near(impact_entity->shields, 200.0f));

    // passHitsToOwner redirects only the entity-side 100-point impact. Player
    // damage still comes from the colliding child's own damage_FLOAT.
    UnitOptions impact_parent_o;
    impact_parent_o.shields = 300.0f;
    auto impact_parent_u = make_unit(impact_parent_o);
    UnitOptions impact_child_o;
    impact_child_o.collides_players = true;
    impact_child_o.pass_hits = true;
    impact_child_o.shields = 10.0f;
    impact_child_o.damage = 9.0f;
    auto impact_child_u = make_unit(impact_child_o);
    deimos::EntityWorld redirected_world;
    redirected_world.members().push_back(make_entity(41, 41, id("ipar"), impact_parent_u));
    redirected_world.members().push_back(make_entity(42, 42, id("ichd"), impact_child_u));
    auto* impact_parent = redirected_world.find_member(41);
    auto* impact_child = redirected_world.find_member(42);
    impact_child->parent = {41, 41};
    impact_child->x = impact_child->y = 100.0f;
    std::map<std::string, const deimos::UnitDefinition*> redirected_defs = {
        {"ipar", &impact_parent_u}, {"ichd", &impact_child_u}
    };
    const auto redirected_provider = [&](deimos::FourCC unit_id) -> const deimos::UnitDefinition* {
        auto it = redirected_defs.find(unit_id.str());
        return it == redirected_defs.end() ? nullptr : it->second;
    };
    deimos::PlayerWorld redirected_players;
    redirected_players.slots()[0] = {4, 100.0f, 100.0f, 1, 5, 4};
    float redirected_player_damage = 0.0f;
    deimos::LegacyPlayerDamageResult redirected_player_result;
    deimos::CompiledPlayerRuntimeDefinition redirected_player_def;
    deimos::LegacyPlayerCollisionCallbacks redirected_callbacks;
    redirected_callbacks.apply_player_damage = [&](
        deimos::PlayerRuntimeSlot& player, float damage, std::uint32_t tick) {
        redirected_player_damage = damage;
        redirected_player_result = deimos::apply_legacy_player_damage(
            player, redirected_player_def, damage, tick);
    };
    const auto redirected_scan = deimos::scan_legacy_player_collisions(
        redirected_world, *impact_child, redirected_players, {320, 240}, 2,
        redirected_provider, rng, redirected_callbacks);
    assert(redirected_scan.events.size() == 1);
    assert(redirected_scan.events[0].entity_damage_target == 41);
    assert(near(impact_parent->shields, 200.0f));
    assert(near(impact_child->shields, 10.0f));
    assert(near(redirected_player_damage, 9.0f));
    assert(redirected_player_result.death_entered);
    assert(redirected_players.slots()[0].status == 3);
    assert(!redirected_scan.events[0].player_remained_active);

    // Non-'none' pickup types never fall through to reciprocal impact. A
    // failed 0x37580-equivalent callback leaves the entity alive; success
    // consumes it with the player's signed owner/index byte.
    UnitOptions pickup_o;
    pickup_o.collides_players = true;
    pickup_o.pickup_type = id("coin");
    pickup_o.pickup_value = 50;
    pickup_o.damage = 99.0f;
    auto pickup_u = make_unit(pickup_o);
    deimos::EntityWorld pickup_world;
    pickup_world.members().push_back(make_entity(43, 43, id("pick"), pickup_u));
    auto* pickup_entity = pickup_world.find_member(43);
    pickup_entity->x = pickup_entity->y = 100.0f;
    deimos::PlayerWorld pickup_players;
    pickup_players.slots()[0] = {4, 100.0f, 100.0f, 7, 5, 4};
    std::map<std::string, const deimos::UnitDefinition*> pickup_defs = {{"pick", &pickup_u}};
    const auto pickup_provider = [&](deimos::FourCC unit_id) -> const deimos::UnitDefinition* {
        auto it = pickup_defs.find(unit_id.str());
        return it == pickup_defs.end() ? nullptr : it->second;
    };
    int pickup_player_damage_calls = 0;
    deimos::LegacyPlayerCollisionCallbacks pickup_callbacks;
    pickup_callbacks.try_pickup = [&](deimos::PlayerRuntimeSlot&, const deimos::EntityRuntime& e) {
        assert(e.behavior.pickup_type == id("coin"));
        assert(e.behavior.pickup_value == 50);
        return false;
    };
    pickup_callbacks.apply_player_damage = [&](deimos::PlayerRuntimeSlot&, float, std::uint32_t) {
        ++pickup_player_damage_calls;
    };
    auto pickup_scan = deimos::scan_legacy_player_collisions(
        pickup_world, *pickup_entity, pickup_players, {320, 240}, 2,
        pickup_provider, rng, pickup_callbacks);
    assert(pickup_scan.pickup_attempts == 1);
    assert(pickup_scan.pickup_consumptions == 0);
    assert(pickup_scan.reciprocal_impacts == 0);
    assert(pickup_entity->lifecycle == deimos::EntityLifecycle::active);
    assert(pickup_player_damage_calls == 0);

    deimos::CompiledPlayerRuntimeDefinition pickup_player_def;
    pickup_callbacks.try_pickup = [&](
        deimos::PlayerRuntimeSlot& player, const deimos::EntityRuntime& entity) {
        return deimos::apply_legacy_player_pickup(player, entity, pickup_player_def).accepted;
    };
    pickup_entity->behavior.destruction_spawn = id("puff");
    deimos::LegacyRemovalContext pickup_removal_context;
    deimos::LegacyRemovalTrace pickup_removal_trace;
    pickup_scan = deimos::scan_legacy_player_collisions(
        pickup_world, *pickup_entity, pickup_players, {320, 240}, 2,
        pickup_provider, rng, pickup_callbacks, 100.0f, 1,
        &pickup_removal_context, &pickup_removal_trace);
    assert(pickup_scan.pickup_consumptions == 1);
    assert(pickup_scan.entity_became_inactive);
    assert(pickup_entity->lifecycle == deimos::EntityLifecycle::destroyed);
    assert(pickup_entity->destroyed_by_owner_index == 7);
    assert(pickup_entity->destruction_effects_processed);
    assert(pickup_entity->consumed_as_player_pickup);
    assert(pickup_players.slots()[0].money == 50);
    assert(pickup_removal_trace.consequences.size() == 1);
    assert(pickup_removal_trace.consequences[0].kind ==
           deimos::LegacyRemovalConsequenceKind::destruction_spawn);
    assert(pickup_removal_trace.consequences[0].resource_id == id("puff"));
    assert(pickup_player_damage_calls == 0);

    // Exact candidate policy: same air/ground domain, opposite harmless class,
    // candidate active/participating, no positive group delay, plus the
    // asymmetric projectile flags recovered at 0x36E8C..0x36EBC.
    UnitOptions projectile_o;
    projectile_o.harmless = true;
    projectile_o.projectile = true;
    projectile_o.hittable = false;
    auto projectile_u = make_unit(projectile_o);
    UnitOptions enemy_o;
    enemy_o.harmless = false;
    enemy_o.projectile = false;
    enemy_o.hittable = true;
    auto enemy_u = make_unit(enemy_o);
    auto projectile = make_entity(10, 10, id("shot"), projectile_u);
    auto enemy = make_entity(20, 20, id("enem"), enemy_u);
    assert(deimos::legacy_collision_candidate_compatible(projectile, enemy));
    enemy.behavior.harmless_to_players = true;
    assert(!deimos::legacy_collision_candidate_compatible(projectile, enemy));
    enemy.behavior.harmless_to_players = false;
    enemy.group_delay_ticks = 1;
    assert(!deimos::legacy_collision_candidate_compatible(projectile, enemy));
    enemy.group_delay_ticks = 0;
    enemy.collision_participating = false;
    assert(!deimos::legacy_collision_candidate_compatible(projectile, enemy));
    enemy.collision_participating = true;
    enemy.behavior.collision_domain = id("grnd");
    assert(!deimos::legacy_collision_candidate_compatible(projectile, enemy));

    // Canonical Entity_HitDelay=1 is a strict '>' gate. Tick 1 is blocked
    // from an initial last-hit tick of zero; tick 2 applies.
    auto damage_target = make_entity(30, 30, id("enem"), enemy_u);
    damage_target.x = 12.5f;
    damage_target.y = 23.75f;
    auto d = deimos::apply_collision_damage(damage_target, enemy_u, 3.0f, 2, 1, rng);
    assert(!d.applied && near(damage_target.shields, 10.0f));
    d = deimos::apply_collision_damage(damage_target, enemy_u, 3.0f, 2, 2, rng);
    assert(d.applied && near(d.shields_before, 10.0f));
    assert(near(d.shields_after, 7.0f) && near(d.absorbed_damage, 3.0f));
    assert(d.collision_glow_due && d.hit_particles_due);
    assert(d.hit_particle_spawn);
    assert(d.hit_particle_spawn->x == 12.5f && d.hit_particle_spawn->y == 23.75f);
    assert(d.hit_particle_spawn->preset == id("tiny"));
    assert(d.hit_particle_spawn->color == 0x7e08u);
    assert(!d.hit_particle_spawn->ground_space);
    assert(!d.hit_particle_executed);
    d = deimos::apply_collision_damage(damage_target, enemy_u, 1.0f, 2, 3, rng);
    assert(!d.applied && near(damage_target.shields, 7.0f));

    // Damage clamps at zero and enters 0x16300 immediately when a destruction
    // context is present. This preserves death-effect/random-bonus RNG order
    // inside the collision call instead of deferring it to the cleanup pass.
    damage_target.behavior.destruction_spawn = id("frag");
    damage_target.behavior.destruction_release_random_bonus = true;
    deimos::LegacyRemovalContext damage_removal_context;
    damage_removal_context.random_bonus.progression_value = 3;
    damage_removal_context.random_bonus_config.percent_thresholds =
        {70, 78, 82, 84, 87, 91, 95, 98, 100};
    damage_removal_context.random_bonus_config.ground_accuracy_reward_percent = 10;
    damage_removal_context.random_bonus_config.minimum_progression_for_highest_bonus = 3;
    damage_removal_context.random_bonus_config.bonus_ids = {
        id("rb01"), id("rb02"), id("rb03"), id("rb04"), id("rb05"),
        id("rb06"), id("rb07"), id("rb08"), id("rb09"), id("rb10")
    };
    deimos::LegacyRemovalTrace damage_removal_trace;
    deimos::LegacyRandom expected_death_rng(rng.seed());
    (void)deimos::choose_inclusive_integer(0, 100, expected_death_rng);
    d = deimos::apply_collision_damage(
        damage_target, enemy_u, 100.0f, 7, 4, rng, 1,
        &damage_removal_context, &damage_removal_trace);
    assert(d.applied && d.entity_destroyed);
    assert(near(d.absorbed_damage, 7.0f));
    assert(damage_target.lifecycle == deimos::EntityLifecycle::destroyed);
    assert(damage_target.destroyed_by_owner_index == 7);
    assert(damage_target.destruction_effects_processed);
    assert(d.score_award == enemy_o.score);
    assert(rng.seed() == expected_death_rng.seed());
    assert(damage_removal_trace.consequences.size() == 2);
    assert(damage_removal_trace.consequences[0].kind ==
           deimos::LegacyRemovalConsequenceKind::destruction_spawn);
    assert(damage_removal_trace.consequences[1].kind ==
           deimos::LegacyRemovalConsequenceKind::random_bonus_spawn);

    // Invulnerability restores old shields after calculating the absorbed
    // amount, exactly as 0x14FD4..0x14FE0 does.
    UnitOptions inv_o;
    inv_o.invulnerable = true;
    auto inv_u = make_unit(inv_o);
    auto inv = make_entity(40, 40, id("invu"), inv_u);
    d = deimos::apply_collision_damage(inv, inv_u, 4.0f, -1, 2, rng);
    assert(d.applied && d.invulnerability_restored_shields);
    assert(near(d.absorbed_damage, 4.0f));
    assert(near(inv.shields, 10.0f));
    assert(inv.lifecycle == deimos::EntityLifecycle::active);

    // On-hit state changes require nonzero configured delay and strict
    // current > lastTransition+delay. Equality does not fire.
    UnitOptions hit_o;
    hit_o.hit_delay = 2;
    hit_o.on_hit = "Hit";
    hit_o.no_glow = true;
    hit_o.collision_spawn = id("oldx");
    hit_o.collision_spawn_delay = 3;
    auto hit_u = make_unit(hit_o);
    auto hit = make_entity(50, 50, id("hitx"), hit_u);
    d = deimos::apply_collision_damage(hit, hit_u, 1.0f, -1, 2, rng, 0);
    assert(d.applied && !d.on_hit_action_due && hit.state.current_state == 0);
    d = deimos::apply_collision_damage(hit, hit_u, 1.0f, -1, 3, rng, 0);
    assert(d.applied && d.on_hit_action_due && hit.state.current_state == 1);
    // 0x14F10 keeps its pre-transition state pointer: even though the live
    // state is now Hit, post-hit glow/spawn facts still come from Initial.
    assert(!d.collision_glow_due);
    assert(d.collision_spawn_due && *d.collision_spawn_due == id("oldx"));

    // Collision spawn equality is due; non-repeat state spawns only once.
    UnitOptions spawn_o;
    spawn_o.collision_spawn = id("boom");
    spawn_o.collision_spawn_delay = 2;
    auto spawn_u = make_unit(spawn_o);
    auto spawn = make_entity(60, 60, id("spwn"), spawn_u);
    d = deimos::apply_collision_damage(spawn, spawn_u, 1.0f, -1, 2, rng, 0);
    assert(d.collision_spawn_due && *d.collision_spawn_due == id("boom"));
    assert(spawn.collision_spawn_count == 1);
    d = deimos::apply_collision_damage(spawn, spawn_u, 1.0f, -1, 4, rng, 0);
    assert(!d.collision_spawn_due && spawn.collision_spawn_count == 1);

    // Preserve the observed second-leg passHitsToOwner quirk: candidate's pass
    // flag tests SELF.parent. Both damage targets therefore resolve to that
    // parent here; the second same-tick damage is then suppressed by HitDelay.
    UnitOptions parent_o;
    parent_o.shields = 20.0f;
    auto parent_u = make_unit(parent_o);
    UnitOptions self_o = projectile_o;
    self_o.pass_hits = true;
    self_o.damage = 2.0f;
    auto self_u = make_unit(self_o);
    UnitOptions cand_o = enemy_o;
    cand_o.pass_hits = true;
    cand_o.damage = 3.0f;
    auto cand_u = make_unit(cand_o);

    std::map<std::string, const deimos::UnitDefinition*> defs = {
        {"pare", &parent_u}, {"self", &self_u}, {"cand", &cand_u}
    };
    const auto provider = [&](deimos::FourCC unit_id) -> const deimos::UnitDefinition* {
        auto it = defs.find(unit_id.str());
        return it == defs.end() ? nullptr : it->second;
    };

    deimos::EntityWorld pair_world;
    pair_world.members().push_back(make_entity(70, 70, id("pare"), parent_u));
    pair_world.members().push_back(make_entity(71, 71, id("self"), self_u));
    pair_world.members().push_back(make_entity(72, 72, id("cand"), cand_u));
    auto* parent = pair_world.find_member(70);
    auto* self = pair_world.find_member(71);
    auto* cand = pair_world.find_member(72);
    self->parent = {70, 70};
    const auto pair = deimos::apply_legacy_collision_pair(
        pair_world, *self, *cand, 2, provider, rng);
    assert(pair.first_damage_target == 70);
    assert(pair.second_damage_target == 70);
    assert(pair.first_damage.applied);
    assert(!pair.second_damage.applied);
    assert(near(parent->shields, 17.0f));

    // Full scan: AABB-overlapping corner candidates are rejected by the exact
    // radial second stage; a true radial overlap applies damage and exits if
    // self is destroyed.
    UnitOptions scan_self_o = projectile_o;
    scan_self_o.shields = 1.0f;
    scan_self_o.damage = 2.0f;
    auto scan_self_u = make_unit(scan_self_o);
    UnitOptions scan_enemy_o = enemy_o;
    scan_enemy_o.damage = 2.0f;
    auto scan_enemy_u = make_unit(scan_enemy_o);
    std::map<std::string, const deimos::UnitDefinition*> scan_defs = {
        {"sslf", &scan_self_u}, {"se01", &scan_enemy_u}, {"se02", &scan_enemy_u}
    };
    const auto scan_provider = [&](deimos::FourCC unit_id) -> const deimos::UnitDefinition* {
        auto it = scan_defs.find(unit_id.str());
        return it == scan_defs.end() ? nullptr : it->second;
    };
    deimos::EntityWorld scan_world;
    scan_world.members().push_back(make_entity(80, 80, id("sslf"), scan_self_u));
    scan_world.members().push_back(make_entity(81, 81, id("se01"), scan_enemy_u));
    scan_world.members().push_back(make_entity(82, 82, id("se02"), scan_enemy_u));
    auto* scan_self = scan_world.find_member(80);
    scan_self->x = 10.0f;
    scan_self->y = 10.0f;
    scan_world.find_member(81)->x = 18.0f;
    scan_world.find_member(81)->y = 16.0f;
    scan_world.find_member(82)->x = 18.0f;
    scan_world.find_member(82)->y = 16.0f;

    auto scan_result = deimos::scan_legacy_entity_collisions(
        scan_world, *scan_self, 2, scan_provider, rng);
    assert(scan_result.aabb_overlaps == 2);
    assert(scan_result.radial_overlaps == 0);
    assert(scan_result.collisions_applied == 0);
    assert(scan_self->lifecycle == deimos::EntityLifecycle::active);

    scan_world.find_member(81)->x = 10.0f;
    scan_world.find_member(81)->y = 10.0f;
    scan_result = deimos::scan_legacy_entity_collisions(
        scan_world, *scan_self, 2, scan_provider, rng);
    assert(scan_result.radial_overlaps == 1);
    assert(scan_result.collisions_applied == 1);
    assert(scan_result.self_became_inactive);
    assert(scan_self->lifecycle == deimos::EntityLifecycle::destroyed);

    return 0;
}
