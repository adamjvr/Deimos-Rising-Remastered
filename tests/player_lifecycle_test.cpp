#include "deimos/player_runtime.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <string>

namespace {

deimos::FourCC id(const char* s) {
    return deimos::FourCC{{s[0], s[1], s[2], s[3]}};
}

bool near(float a, float b) {
    return std::fabs(a - b) < 0.0001f;
}

} // namespace

int main() {
    // These values are deliberately distinct so each 0x2A150 compiled offset
    // can be regression-bound to its serialized Player Definition key.
    const std::string text = R"(#name_STR <Lifecycle Test>
#defaultShieldPercentage_INT <73.000000>
#shieldWarningPercentage_INT <15.000000>
#shieldBaseHitPercentage_INT <15.000000>
#shieldHitDelay_INT <1>
#life_MaxNum_INT <10>
#life_NumInitial_INT <3>
#life_Spawn_ID <noel>
#gameOverTime_INT <20>
#dyingTime_INT <80>
#finalDyingTime_INT <40>
#entry_soloStartX_INT <208>
#entry_soloStartY_INT <330>
#entry_multiStartX_INT <104>
#entry_multiStartY_INT <331>
#entry_Spawn_ID <plen>
#entry_InitialDelay_INT <55>
#entry_InvulnerabilityTime_INT <60>
#death_Spawn_ID <plde>
#active_SpawnOnHit_ID <plsh>
#active_ShieldWarningObject_ID <nosw>
#active_DefenceBonusObject_ID <nodb>
)";

    std::string error;
    const auto doc = deimos::parse_tagged_text(text, &error);
    assert(doc);
    const auto parsed = deimos::parse_player_definition_document(*doc, &error);
    assert(parsed);
    const auto def = deimos::compile_player_runtime_definition(*parsed);

    assert(def.game_over_time_ticks == 20);                 // compiled +0x80
    assert(def.dying_time_ticks == 80);                     // +0x84
    assert(def.final_dying_time_ticks == 40);               // +0x88
    assert(def.entry_invulnerability_time_ticks == 60);     // +0x8C
    assert(def.entry_solo_start_x == 208);                  // +0x90
    assert(def.entry_solo_start_y == 330);                  // +0x94
    assert(def.entry_multi_start_x == 104);                 // +0x98
    assert(def.entry_multi_start_y == 331);                 // +0x9C
    assert(def.entry_spawn == id("plen"));                  // +0xA0
    assert(def.entry_initial_delay_ticks == 55);            // +0xB8
    assert(def.death_spawn == id("plde"));                  // +0xBC

    // Status 2 waits THROUGH equality. At tick 156 (>100+55) it enters active,
    // chooses the +0xCD-selected solo coordinates and zeroes velocity exactly
    // as 0x29DA8..0x29DC4 does.
    deimos::PlayerRuntimeSlot player;
    player.enabled = true;
    player.status = static_cast<int>(deimos::LegacyPlayerStatus::waiting);
    player.status_since_tick = 100;
    player.use_solo_entry_position = true;
    player.velocity_x = 9.0f;
    player.velocity_y = -9.0f;
    auto r = deimos::advance_legacy_player_lifecycle(player, def, 155);
    assert(r.active_entry_waiting && !r.respawned);
    assert(player.status == static_cast<int>(deimos::LegacyPlayerStatus::waiting));
    r = deimos::advance_legacy_player_lifecycle(player, def, 156);
    assert(r.respawned);
    assert(r.status_before == static_cast<int>(deimos::LegacyPlayerStatus::waiting));
    assert(r.status_after == static_cast<int>(deimos::LegacyPlayerStatus::active));
    assert(r.respawn_position && near(r.respawn_position->x, 208.0f) && near(r.respawn_position->y, 330.0f));
    assert(r.entry_spawn_due == id("plen"));
    assert(near(player.velocity_x, 0.0f) && near(player.velocity_y, 0.0f));
    assert(player.status_since_tick == 156);

    // +0xCD clear selects the multiplayer coordinate pair.
    player.status = static_cast<int>(deimos::LegacyPlayerStatus::waiting);
    player.status_since_tick = 200;
    player.use_solo_entry_position = false;
    r = deimos::advance_legacy_player_lifecycle(player, def, 256);
    assert(r.respawned);
    assert(near(player.x, 104.0f) && near(player.y, 331.0f));

    // Status 4 invulnerability uses compiled +0x8C, not serialized-field order.
    // Equality still holds invulnerability; +0xCF and the external 0x5CF0 gate
    // independently suppress expiry.
    player.status = static_cast<int>(deimos::LegacyPlayerStatus::active);
    player.status_since_tick = 300;
    player.invulnerable = true;
    player.invulnerability_latched = false;
    r = deimos::advance_legacy_player_lifecycle(player, def, 360);
    assert(!r.invulnerability_cleared && player.invulnerable);
    r = deimos::advance_legacy_player_lifecycle(player, def, 361, true, true);
    assert(!r.invulnerability_cleared && player.invulnerable);
    player.invulnerability_latched = true;
    r = deimos::advance_legacy_player_lifecycle(player, def, 361);
    assert(!r.invulnerability_cleared && player.invulnerable);
    player.invulnerability_latched = false;
    r = deimos::advance_legacy_player_lifecycle(player, def, 361);
    assert(r.invulnerability_cleared && !player.invulnerable);

    // Ordinary dying state uses dyingTime=80. Equality does not consume a life.
    // One tick later it decrements, respawns, restores default shield, and clears
    // the two hit clocks + warning latch while preserving the death invulnerability.
    player.enabled = true;
    player.use_solo_entry_position = true;
    player.status = static_cast<int>(deimos::LegacyPlayerStatus::dying);
    player.status_since_tick = 400;
    player.lives = 3;
    player.shield_percentage = -5.0f;
    player.last_shield_hit_tick = 77;
    player.last_spawn_on_hit_tick = 88;
    player.shield_warning_latched = true;
    player.invulnerable = true;
    r = deimos::advance_legacy_player_lifecycle(player, def, 480);
    assert(!r.life_decremented && player.lives == 3);
    r = deimos::advance_legacy_player_lifecycle(player, def, 481);
    assert(r.life_decremented && r.respawned);
    assert(player.lives == 2);
    assert(player.status == static_cast<int>(deimos::LegacyPlayerStatus::active));
    assert(near(player.shield_percentage, 73.0f));
    assert(player.last_shield_hit_tick == 0 && player.last_spawn_on_hit_tick == 0);
    assert(!player.shield_warning_latched);
    assert(player.invulnerable);

    // The fifth-argument gate can expire a death without consuming a life.
    player.status = static_cast<int>(deimos::LegacyPlayerStatus::dying);
    player.status_since_tick = 500;
    player.lives = 2;
    r = deimos::advance_legacy_player_lifecycle(player, def, 581, false);
    assert(!r.life_decremented && r.respawned && player.lives == 2);

    // Exactly one remaining life selects finalDyingTime=40. After that final
    // decrement, status 1 begins but +0xC4 remains enabled until gameOverTime.
    player.status = static_cast<int>(deimos::LegacyPlayerStatus::dying);
    player.status_since_tick = 600;
    player.lives = 1;
    r = deimos::advance_legacy_player_lifecycle(player, def, 640);
    assert(!r.life_decremented && player.status == static_cast<int>(deimos::LegacyPlayerStatus::dying));
    r = deimos::advance_legacy_player_lifecycle(player, def, 641);
    assert(r.life_decremented && r.game_over_entered);
    assert(player.lives == 0);
    assert(player.status == static_cast<int>(deimos::LegacyPlayerStatus::game_over));
    assert(player.enabled && player.status_since_tick == 641);

    // Status 1 is itself a strict timer. Equality remains enabled, then the
    // next tick clears original player +0xC4.
    r = deimos::advance_legacy_player_lifecycle(player, def, 661);
    assert(!r.disabled_after_game_over && player.enabled);
    r = deimos::advance_legacy_player_lifecycle(player, def, 662);
    assert(r.disabled_after_game_over && !player.enabled);

    // Once +0xC4 is clear the whole switch is bypassed, even if status changes.
    player.status = static_cast<int>(deimos::LegacyPlayerStatus::waiting);
    const float frozen_x = player.x;
    r = deimos::advance_legacy_player_lifecycle(player, def, 1000);
    assert(!r.respawned && !player.enabled && near(player.x, frozen_x));

    std::cout << "player lifecycle tests passed\n";
    return 0;
}
