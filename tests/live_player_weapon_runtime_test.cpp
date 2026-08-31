#include "deimos/live_player_weapon_runtime.hpp"

#include <cstdlib>
#include <iostream>

namespace {

using namespace deimos;

FourCC id(char a, char b, char c, char d) { return FourCC{{a,b,c,d}}; }

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

LivePlayerWeaponSlot ion() {
    LivePlayerWeaponSlot out;
    out.id = id('a','i','i','c');
    out.name = "Air - Ion Cannon";
    out.type = id('P','E','A','A');
    out.default_marker = id('D','E','A','A');
    out.minimum_level_available = 1;
    out.maximum_level_available = 9999;
    out.auto_repeat = false;
    out.delay_between_launches = 4;
    out.powerup_air_time_until_activation = 15;
    out.powerup_air_activation_spawn_id = id('i','c','p','o');
    out.powerup_air_time_between_power_level_changes = 2;
    out.powerup_air_max_power_level = 20;
    out.powerup_air_overload_time = 180;
    out.powerup_air_release_spawn_id = id('i','c','p','s');
    out.powerup_air_time_between_release_spawns = 1;
    out.powerup_air_do_release_on_max_power_level = false;
    out.player1_appearance_face = id('p','l','1','o');
    out.score_bar_preview_face = id('w','e','s','y');
    out.score_bar_preview_frame = 0;
    out.spawns = {
        WeaponSpawn{"left", id('i','c','b',' '), -5, 0, false, 0},
        WeaponSpawn{"flash", id('i','c','b','f'), 0, -8, false, 0},
        WeaponSpawn{"right", id('i','c','b',' '), 4, 0, false, 0},
    };
    return out;
}

LivePlayerWeaponSlot photon() {
    LivePlayerWeaponSlot out;
    out.id = id('a','i','p','b');
    out.name = "Air - Photon Beam";
    out.type = id('P','E','A','A');
    out.minimum_level_available = 2;
    out.maximum_level_available = 9999;
    out.score_bar_preview_face = id('w','e','s','y');
    out.score_bar_preview_frame = 2;
    return out;
}

LivePlayerWeaponSlot plasma() {
    LivePlayerWeaponSlot out;
    out.id = id('p','l','b','o');
    out.name = "Ground - Plasma Bomb";
    out.type = id('P','E','A','G');
    out.default_marker = id('D','E','A','G');
    out.minimum_level_available = 0;
    out.maximum_level_available = 9999;
    out.auto_repeat = false;
    out.delay_between_launches = 8;
    out.crosshair_face = id('p','b','t','a');
    out.crosshair_frame = 0;
    out.crosshair_locked_face = id('p','b','t','a');
    out.crosshair_locked_frame = 1;
    out.crosshair_x_offset = 0;
    out.crosshair_y_offset = -121;
    out.spawns = { WeaponSpawn{"bomb", id('p','l','b','m'), 0, 6, false, 0} };
    return out;
}

} // namespace

int main() {
    using namespace deimos;

    LivePlayerWeaponCatalog catalog;
    catalog.air = {ion(), photon()};
    catalog.ground = {plasma()};
    catalog.default_air = 0;
    catalog.default_ground = 0;

    LivePlayerWeaponState state;
    initialize_live_player_weapon_state(state, catalog, 1);
    require(state.selected_air == 0, "level 1 selects canonical Ion Cannon");
    require(state.selected_ground == 0, "default ground weapon selected");
    require(selected_live_ground_weapon(catalog, state)->crosshair_face == id('p','b','t','a'),
            "Plasma Bomb preserves canonical pbta crosshair face");
    require(selected_live_ground_weapon(catalog, state)->crosshair_frame == 0 &&
            selected_live_ground_weapon(catalog, state)->crosshair_locked_frame == 1,
            "Plasma Bomb preserves normal/locked crosshair frames");
    require(selected_live_ground_weapon(catalog, state)->crosshair_x_offset == 0 &&
            selected_live_ground_weapon(catalog, state)->crosshair_y_offset == -121,
            "Plasma Bomb preserves canonical targeting offset");
    require(selected_live_air_weapon(catalog, state)->score_bar_preview_face == id('w','e','s','y'),
            "selected weapon preserves score-bar preview face");
    require(selected_live_air_weapon(catalog, state)->score_bar_preview_frame == 0,
            "selected weapon preserves score-bar preview frame");

    PlayerRuntimeSlot player;
    player.player_index = 0;
    player.x = 208.0f;
    player.y = 330.0f;

    LivePlayerWeaponInput fire;
    fire.fire_air = true;
    auto result = advance_live_player_weapons(catalog, state, fire, player, 0, 1);
    require(result.air_launched && result.air_launch.has_value(), "Ion Cannon launches on rising edge");
    require(result.air_launch->requests.size() == 3, "Ion Cannon emits three serialized spawns");
    require(result.air_launch->requests[0].unit_id == id('i','c','b',' '), "left Ion bullet id");
    require(result.air_launch->requests[0].x == 203.0f && result.air_launch->requests[0].y == 330.0f,
            "left Ion bullet offset");
    require(result.air_launch->requests[1].unit_id == id('i','c','b','f'), "Ion flash id");
    require(result.air_launch->requests[1].x == 208.0f && result.air_launch->requests[1].y == 322.0f,
            "Ion flash offset");
    require(result.air_launch->requests[2].x == 212.0f && result.air_launch->requests[2].y == 330.0f,
            "right Ion bullet offset");
    for (const auto& request : result.air_launch->requests) {
        require(request.player_owner_index == 0, "weapon spawns preserve player ownership");
    }

    result = advance_live_player_weapons(catalog, state, fire, player, 4, 1);
    require(!result.air_launched, "non-auto-repeat weapon does not relaunch while held");

    LivePlayerWeaponInput released;
    (void)advance_live_player_weapons(catalog, state, released, player, 4, 1);
    result = advance_live_player_weapons(catalog, state, fire, player, 4, 1);
    require(result.air_launched, "release/repress at delay boundary launches");

    LivePlayerWeaponInput sw;
    sw.switch_air = true;
    result = advance_live_player_weapons(catalog, state, sw, player, 5, 1);
    require(!result.air_switched && state.selected_air == 0,
            "level-2-only air weapon is skipped on level 1");
    (void)advance_live_player_weapons(catalog, state, {}, player, 6, 2);
    result = advance_live_player_weapons(catalog, state, sw, player, 7, 2);
    require(result.air_switched && state.selected_air == 1,
            "weapon switch selects newly available air weapon");

    LivePlayerWeaponInput ground;
    ground.fire_ground = true;
    result = advance_live_player_weapons(catalog, state, ground, player, 8, 1);
    require(result.ground_launched && result.ground_launch && result.ground_launch->requests.size() == 1,
            "ground weapon launch emits request");
    require(result.ground_launch->requests.front().y == 336.0f,
            "ground weapon serialized offset applied");


    // Serialized hold-to-charge path: normal shot on press, activation after
    // 15 ticks, then one power level every 2 ticks. Releasing schedules one
    // canonical icps release spawner per attained power level and resets the
    // HUD power percentage.
    LivePlayerWeaponState charge_state;
    initialize_live_player_weapon_state(charge_state, catalog, 1);
    result = advance_live_player_weapons(catalog, charge_state, fire, player, 100, 1);
    require(result.air_launched, "charge hold still fires ordinary shot on press");
    for (std::uint32_t tick = 101; tick < 115; ++tick) {
        result = advance_live_player_weapons(catalog, charge_state, fire, player, tick, 1);
        require(!result.air_powerup_activated, "charge does not activate before configured delay");
    }
    result = advance_live_player_weapons(catalog, charge_state, fire, player, 115, 1);
    require(result.air_powerup_activated, "charge activates at configured hold delay");
    require(result.powerup_requests.size() == 1 &&
            result.powerup_requests.front().unit_id == id('i','c','p','o'),
            "charge activation constructs canonical icpo effect");
    require(result.air_power_percentage == 0.0f, "power meter begins at zero on activation");

    result = advance_live_player_weapons(catalog, charge_state, fire, player, 119, 1);
    require(result.air_power_level == 2, "charge advances one level per configured interval");
    require(result.air_power_percentage == 10.0f, "charge exposes percentage for HUD power meter");

    // Shipped handler 0x3B3C0 does not consume serialized OverloadTime as an
    // automatic-release timer. PPC Lab with OverloadTime=1 and DoReleaseOnMax
    // false remains in charged state at max/100 even far beyond that time.
    auto no_overload_catalog = catalog;
    no_overload_catalog.air[0].powerup_air_overload_time = 1;
    no_overload_catalog.air[0].powerup_air_time_between_power_level_changes = 1;
    no_overload_catalog.air[0].powerup_air_max_power_level = 2;
    no_overload_catalog.air[0].powerup_air_do_release_on_max_power_level = false;
    LivePlayerWeaponState no_overload_state;
    initialize_live_player_weapon_state(no_overload_state, no_overload_catalog, 1);
    (void)advance_live_player_weapons(no_overload_catalog, no_overload_state, fire, player, 200, 1);
    for (std::uint32_t tick = 201; tick <= 240; ++tick) {
        result = advance_live_player_weapons(no_overload_catalog, no_overload_state, fire, player, tick, 1);
    }
    require(result.air_power_level == 2, "charge clamps at max power after serialized overload time");
    require(result.air_power_percentage == 100.0f, "max charge remains visible at 100 percent");
    require(!result.air_powerup_released && result.powerup_requests.empty(),
            "OverloadTime alone does not auto-release shipped 0x3B3C0 handler");
    require(!result.air_powerup_overloaded, "handler does not emit fabricated overload state");

    result = advance_live_player_weapons(catalog, charge_state, released, player, 120, 1);
    require(result.air_powerup_released, "releasing charged weapon starts release stream");
    require(result.powerup_requests.size() == 1 &&
            result.powerup_requests.front().unit_id == id('i','c','p','s'),
            "first charged release spawner is emitted immediately");
    require(result.air_power_percentage == 0.0f, "HUD power meter resets on release");
    result = advance_live_player_weapons(catalog, charge_state, released, player, 121, 1);
    require(result.powerup_requests.size() == 1 &&
            result.powerup_requests.front().unit_id == id('i','c','p','s'),
            "remaining charged release spawner follows serialized cadence");

    std::cout << "live player weapon runtime PASS\n";
    return 0;
}
