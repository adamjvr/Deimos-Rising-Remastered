#include "deimos/player_runtime.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <string>

namespace {

deimos::FourCC id(const char* s) {
    return deimos::FourCC{{s[0], s[1], s[2], s[3]}};
}

deimos::EntityRuntime pickup(const char* type, int value) {
    deimos::EntityRuntime e;
    e.behavior.pickup_type = id(type);
    e.behavior.pickup_value = value;
    return e;
}

bool nearly(float a, float b) {
    return std::fabs(a - b) < 0.0001f;
}

} // namespace

int main() {
    // Source-format Player Definition -> exact semantic subset used by
    // PPC 0x27100 / 0x37580.
    const std::string text = R"(#name_STR <Player 1>
#spriteScoreBar_ID <play>
#spriteScoreBarFrame_INT <0>
#spriteScoreBarPower_ID <shme>
#spriteScoreBarPowerFrame_INT <1>
#spriteScoreBarShield_ID <shme>
#spriteScoreBarShieldFrame_INT <0>
#defaultShieldPercentage_INT <100.000000>
#shieldWarningPercentage_INT <15.000000>
#shieldBaseHitPercentage_INT <15.000000>
#shieldHitDelay_INT <1>
#life_MaxNum_INT <10>
#life_NumInitial_INT <3>
#life_InitialRequiredScore_INT <10000>
#life_AdditionalRequiredScore_INT <30000>
#life_Spawn_ID <noel>
#death_Spawn_ID <plde>
#active_SpawnOnHit_ID <plsh>
#active_ShieldWarningObject_ID <nosw>
#active_DefenceBonusObject_ID <nodb>
)";
    std::string error;
    auto doc = deimos::parse_tagged_text(text, &error);
    assert(doc);
    auto parsed = deimos::parse_player_definition_document(*doc, &error);
    assert(parsed);
    deimos::NamedTable<float> game_floats(183);
    for (std::size_t i = 0; i < game_floats.size(); ++i) {
        game_floats[i] = {"unused", 0.0f};
    }
    game_floats[161] = {"Player_ImpactDamageToEntities", 100.0f};
    game_floats[162] = {"Player_DelayBetweenHitSpawns", 10.9f};
    game_floats[167] = {"Entity_HitDelay", 1.9f};
    game_floats[182] = {"Player_ExtraLifeScoreAdjustment", 10000.9f};
    const auto globals = deimos::compile_legacy_player_runtime_globals(game_floats, &error);
    assert(globals);
    assert(nearly(globals->impact_damage_to_entities, 100.0f));
    assert(globals->delay_between_hit_spawns == 10);
    assert(globals->entity_hit_delay_ticks == 1);
    game_floats[162].first = "wrong";
    assert(!deimos::compile_legacy_player_runtime_globals(game_floats, &error));
    game_floats[162].first = "Player_DelayBetweenHitSpawns";
    const auto score_globals = deimos::compile_legacy_player_score_globals(game_floats, &error);
    assert(score_globals && score_globals->extra_life_score_adjustment == 10000);
    game_floats[182].first = "wrong";
    assert(!deimos::compile_legacy_player_score_globals(game_floats, &error));
    game_floats[182].first = "Player_ExtraLifeScoreAdjustment";

    deimos::NamedTable<deimos::FourCC> objects(6);
    objects[0] = {"Player 1", id("pl01")};
    objects[1] = {"Player 2", id("pl02")};
    objects[2] = {"MoneyUnit_50", id("calg")};
    objects[3] = {"MoneyUnit_10", id("cals")};
    objects[4] = {"MoneyUnit_5", id("casg")};
    objects[5] = {"MoneyUnit_1", id("cass")};
    const auto resources = deimos::compile_legacy_player_runtime_resources(objects, &error);
    assert(resources);
    assert(resources->money_50 == id("calg"));
    assert(resources->money_10 == id("cals"));
    assert(resources->money_5 == id("casg"));
    assert(resources->money_1 == id("cass"));

    const auto def = deimos::compile_player_runtime_definition(*parsed);
    assert(def.score_bar_face == id("play") && def.score_bar_frame == 0);
    assert(def.score_bar_power_face == id("shme") && def.score_bar_power_frame == 1);
    assert(def.score_bar_shield_face == id("shme") && def.score_bar_shield_frame == 0);
    assert(def.life_initial_required_score == 10000);
    assert(def.life_additional_required_score == 30000);
    assert(nearly(def.default_shield_percentage, 100.0f));
    assert(nearly(def.shield_warning_percentage, 15.0f));
    assert(nearly(def.shield_base_hit_percentage, 15.0f));
    assert(def.shield_hit_delay_ticks == 1);
    assert(def.life_max == 10 && def.life_initial == 3);
    assert(def.life_spawn == id("noel"));
    assert(def.death_spawn == id("plde"));
    assert(def.active_spawn_on_hit == id("plsh"));
    assert(def.active_shield_warning_object == id("nosw"));
    assert(def.active_defence_bonus_object == id("nodb"));

    deimos::PlayerRuntimeSlot player;
    player.status = 4;
    player.player_index = 0;
    deimos::initialize_legacy_player_gameplay(player, def);
    assert(nearly(player.shield_percentage, 100.0f));
    assert(player.lives == 3);
    assert(player.money == 0);
    assert(player.power_multiplier == 1);
    assert(player.score == 0);
    assert(player.next_extra_life_score == 10000);
    assert(player.extra_life_score_adjustment == 0);

    // PPC 0x29A10 score awards use the live bonus multiplier. Equality at the
    // threshold does not award a life; the test is strictly newScore > threshold.
    auto score_result = deimos::apply_legacy_player_score(player, def, *score_globals, 10000);
    assert(score_result.score_after == 10000 && player.lives == 3);
    score_result = deimos::apply_legacy_player_score(player, def, *score_globals, 1);
    assert(score_result.extra_life_threshold_crossed);
    assert(player.score == 10001 && player.lives == 4);
    assert(score_result.life_spawn_due == id("noel"));
    assert(player.next_extra_life_score == 40000);
    assert(player.extra_life_score_adjustment == 10000);

    // Only one threshold is consumed per award even when a large score jump
    // crosses several nominal thresholds. Multiplier 2 doubles the award.
    player.power_multiplier = 2;
    score_result = deimos::apply_legacy_player_score(player, def, *score_globals, 50000);
    assert(score_result.awarded_points == 100000);
    assert(player.score == 110001 && player.lives == 5);
    assert(player.next_extra_life_score == 80000); // 40000 + 30000 + prior 10000
    assert(player.extra_life_score_adjustment == 20000);

    // r5!=0/raw branch bypasses the multiplier and replaces +0xA0 with
    // score_after + Game[182] for positive awards.
    score_result = deimos::apply_legacy_player_score(player, def, *score_globals, 9, true);
    assert(score_result.awarded_points == 9 && player.score == 110010);
    assert(player.extra_life_score_adjustment == 120010);

    // Disabled players ignore 0x29A10 entirely.
    player.enabled = false;
    score_result = deimos::apply_legacy_player_score(player, def, *score_globals, 100);
    assert(!score_result.applied && player.score == 110010);
    player.enabled = true;

    // Reset the independent pickup test state.
    player.score = 0; player.next_extra_life_score = 10000; player.extra_life_score_adjustment = 0;
    player.lives = 3; player.power_multiplier = 1;

    // Coin: value zero skips both the obfuscated money add and the feedback
    // helper; nonzero adds semantic money and requests the feedback call.
    auto zero_coin = pickup("coin", 0);
    auto p = deimos::apply_legacy_player_pickup(player, zero_coin, def);
    assert(p.accepted && player.money == 0 && !p.feedback_due);
    auto gold = pickup("coin", 50);
    p = deimos::apply_legacy_player_pickup(player, gold, def);
    assert(p.accepted && p.money_before == 0 && p.money_after == 50);
    assert(player.money == 50 && p.feedback_due);

    // Multiplier jump table: 1->2->3->4->5->10, then no-op at 10.
    auto mult = pickup("mult", 0);
    for (int expected : {2, 3, 4, 5, 10}) {
        p = deimos::apply_legacy_player_pickup(player, mult, def);
        assert(player.power_multiplier == expected);
    }
    p = deimos::apply_legacy_player_pickup(player, mult, def);
    assert(player.power_multiplier == 10);

    // Extra life is capped but always accepted. A successful increment emits
    // life_Spawn_ID; a maxed player consumes the pickup without a spawn.
    auto extra = pickup("exli", 1);
    player.lives = 9;
    p = deimos::apply_legacy_player_pickup(player, extra, def);
    assert(player.lives == 10 && p.spawn_due == id("noel"));
    p = deimos::apply_legacy_player_pickup(player, extra, def);
    assert(player.lives == 10 && !p.spawn_due);

    // Shield pickup adds its integer value and clamps semantic percentage.
    auto shield = pickup("shie", 20);
    player.shield_percentage = 90.0f;
    p = deimos::apply_legacy_player_pickup(player, shield, def);
    assert(nearly(player.shield_percentage, 100.0f));
    auto negative_shield = pickup("shie", -150);
    p = deimos::apply_legacy_player_pickup(player, negative_shield, def);
    assert(nearly(player.shield_percentage, 0.0f));

    // air/grnd share the legacy +0xCE gate: invulnerable rejects them; clear
    // invulnerability accepts them. 'spec' and unknown values are no-op accepts.
    auto air = pickup("air ", 0);
    player.invulnerable = true;
    p = deimos::apply_legacy_player_pickup(player, air, def);
    assert(!p.accepted);
    player.invulnerable = false;
    p = deimos::apply_legacy_player_pickup(player, air, def);
    assert(p.accepted);
    auto spec = pickup("spec", 123);
    assert(deimos::apply_legacy_player_pickup(player, spec, def).accepted);

    // Player damage: tick 0 is blocked by the initial last-hit=0 + delay=1
    // gate. Tick 1 passes and scales incoming damage by 15 percentage points.
    deimos::initialize_legacy_player_gameplay(player, def);
    player.status = 4;
    auto d = deimos::apply_legacy_player_damage(player, def, 1.0f, 0, 10);
    assert(!d.processed && d.blocked_by_hit_delay);
    d = deimos::apply_legacy_player_damage(player, def, 1.0f, 1, 10);
    assert(d.processed && !d.blocked_by_hit_delay);
    assert(nearly(d.scaled_shield_damage, 15.0f));
    assert(nearly(player.shield_percentage, 85.0f));
    assert(player.shield_hit_latched);
    assert(d.hit_glow_due);
    assert(!d.spawn_on_hit_due); // 1 < initial 0 + Game[162] 10

    // Invulnerability is tested AFTER the hit tick is written. It bypasses
    // shield subtraction but not glow; clearing invulnerability at the same
    // tick still leaves the next hit blocked by the newly written hit tick.
    player.invulnerable = true;
    const float invuln_shield = player.shield_percentage;
    d = deimos::apply_legacy_player_damage(player, def, 1.0f, 2, 10);
    assert(d.processed && d.invulnerability_bypassed_shield_damage);
    assert(nearly(player.shield_percentage, invuln_shield));
    assert(d.hit_glow_due);
    player.invulnerable = false;
    d = deimos::apply_legacy_player_damage(player, def, 1.0f, 2, 10);
    assert(!d.processed && d.blocked_by_hit_delay);

    // Hit-spawn and shield-warning gates are independent. At tick 10 a small
    // positive hit can cross the warning threshold and satisfy the fixed
    // Player_DelayBetweenHitSpawns=10 gate in the same call.
    player.last_shield_hit_tick = 0;
    player.last_spawn_on_hit_tick = 0;
    player.shield_warning_latched = false;
    player.shield_percentage = 20.0f;
    d = deimos::apply_legacy_player_damage(player, def, 0.5f, 10, 10);
    assert(nearly(player.shield_percentage, 12.5f));
    assert(d.spawn_on_hit_due == id("plsh"));
    assert(d.shield_warning_due == id("nosw"));
    assert(player.shield_warning_latched);

    // Exactly zero shield survives; death is strictly shield < 0.
    player.shield_percentage = 30.0f;
    player.last_shield_hit_tick = 0;
    d = deimos::apply_legacy_player_damage(player, def, 2.0f, 20, 10);
    assert(nearly(player.shield_percentage, 0.0f));
    assert(!d.death_entered && player.status == 4);

    // Immediate death-entry 0x27E50 sets status 3, invulnerability and time,
    // emits the death spawn, decomposes held money, and clears money. It does
    // NOT decrement lives; that belongs to the later death/respawn state.
    player.shield_percentage = 1.0f;
    player.money = 76;
    player.lives = 4;
    player.last_shield_hit_tick = 0;
    d = deimos::apply_legacy_player_damage(player, def, 1.0f, 30, 10, &*resources);
    assert(d.death_entered);
    assert(d.death_spawn_due == id("plde"));
    assert(player.status == 3 && player.status_since_tick == 30);
    assert(player.invulnerable);
    assert(player.money == 0 && d.money_before_death == 76);
    assert(player.lives == 4);
    assert(d.money_drops.size() == 4);
    assert(d.money_drops[0].denomination == 50 && d.money_drops[0].count == 1 && d.money_drops[0].spawn_id == id("calg"));
    assert(d.money_drops[1].denomination == 10 && d.money_drops[1].count == 2 && d.money_drops[1].spawn_id == id("cals"));
    assert(d.money_drops[2].denomination == 5 && d.money_drops[2].count == 1 && d.money_drops[2].spawn_id == id("casg"));
    assert(d.money_drops[3].denomination == 1 && d.money_drops[3].count == 1 && d.money_drops[3].spawn_id == id("cass"));

    std::cout << "player runtime tests passed\n";
    return 0;
}
