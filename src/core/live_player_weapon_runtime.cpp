#include "deimos/live_player_weapon_runtime.hpp"

#include <algorithm>

namespace deimos {
namespace {

constexpr FourCC fourcc(char a, char b, char c, char d) {
    return FourCC{{a,b,c,d}};
}

bool available(const LivePlayerWeaponSlot& slot, int level_number) {
    return level_number >= slot.minimum_level_available &&
           level_number <= slot.maximum_level_available;
}

std::size_t first_available(
    const std::vector<LivePlayerWeaponSlot>& slots,
    std::size_t preferred,
    int level_number) {
    if (preferred < slots.size() && available(slots[preferred], level_number)) return preferred;
    for (std::size_t i = 0; i < slots.size(); ++i) {
        if (available(slots[i], level_number)) return i;
    }
    return 0;
}

std::size_t next_available(
    const std::vector<LivePlayerWeaponSlot>& slots,
    std::size_t current,
    int level_number) {
    if (slots.empty()) return 0;
    for (std::size_t step = 1; step <= slots.size(); ++step) {
        const auto index = (current + step) % slots.size();
        if (available(slots[index], level_number)) return index;
    }
    return current < slots.size() ? current : 0;
}

bool launch_due(
    const LivePlayerWeaponSlot& slot,
    bool down,
    bool was_down,
    const std::optional<std::uint32_t>& last_tick,
    std::uint32_t current_tick) {
    if (!down) return false;
    if (!slot.auto_repeat && was_down) return false;
    if (!last_tick) return true;
    const auto delay = static_cast<std::uint32_t>(std::max(0, slot.delay_between_launches));
    return static_cast<std::int32_t>(current_tick - *last_tick) >= static_cast<std::int32_t>(delay);
}

LivePlayerWeaponLaunch build_launch(
    const LivePlayerWeaponSlot& slot,
    const PlayerRuntimeSlot& player,
    bool ground) {
    LivePlayerWeaponLaunch out;
    out.weapon_id = slot.id;
    out.ground_weapon = ground;
    out.requests.reserve(slot.spawns.size());
    for (const auto& spawn : slot.spawns) {
        SpawnRequestSeed request;
        request.unit_id = spawn.unit_id;
        request.x = player.x + static_cast<float>(spawn.x);
        request.y = player.y + static_cast<float>(spawn.y);
        request.heading_is_set = spawn.set_heading;
        request.heading_degrees = spawn.angle_degrees;
        request.player_owner_index = player.player_index;
        request.initial_velocity_multiplier = 1.0f;
        out.requests.push_back(request);
    }
    return out;
}

bool present(FourCC id) {
    return id != FourCC{} && id != fourcc('n','o','n','e');
}

bool has_air_powerup(const LivePlayerWeaponSlot& slot) {
    return slot.powerup_air_time_until_activation > 0 &&
           slot.powerup_air_max_power_level > 0 &&
           slot.powerup_air_time_between_power_level_changes > 0 &&
           present(slot.powerup_air_activation_spawn_id) &&
           present(slot.powerup_air_release_spawn_id);
}

SpawnRequestSeed build_powerup_request(
    FourCC unit_id,
    const PlayerRuntimeSlot& player) {
    SpawnRequestSeed request;
    request.unit_id = unit_id;
    request.x = player.x;
    request.y = player.y;
    request.player_owner_index = player.player_index;
    request.initial_velocity_multiplier = 1.0f;
    return request;
}

float power_percentage(const LivePlayerWeaponSlot& slot, int level) {
    if (slot.powerup_air_max_power_level <= 0) return 0.0f;
    return 100.0f * static_cast<float>(std::clamp(level, 0, slot.powerup_air_max_power_level)) /
           static_cast<float>(slot.powerup_air_max_power_level);
}

void begin_powerup_hold(LivePlayerWeaponState& state, std::uint32_t current_tick) {
    state.air_hold_started_tick = current_tick;
    state.air_powerup_activated_tick.reset();
    state.air_power_level = 0;
}

void clear_powerup_hold(LivePlayerWeaponState& state) {
    state.air_hold_started_tick.reset();
    state.air_powerup_activated_tick.reset();
    state.air_power_level = 0;
}

void schedule_powerup_release(
    const LivePlayerWeaponSlot& slot,
    LivePlayerWeaponState& state,
    std::uint32_t current_tick,
    LivePlayerWeaponStepResult& result) {
    if (!state.air_powerup_activated_tick || !present(slot.powerup_air_release_spawn_id)) {
        clear_powerup_hold(state);
        return;
    }

    // The serialized release fields describe a repeated release-spawn stream.
    // The exact outer PPC caller is still under recovery; using one release
    // spawner per attained power level preserves the data's explicit
    // time-between-release-spawns contract and produces the intended charged
    // projectile wall without inventing new Unit types.
    state.pending_air_release_spawns = std::max(1, state.air_power_level);
    state.pending_air_release_spawn_id = slot.powerup_air_release_spawn_id;
    state.next_air_release_spawn_tick = current_tick;
    result.air_powerup_released = true;
    clear_powerup_hold(state);
}

void emit_due_powerup_release_spawns(
    const LivePlayerWeaponSlot& slot,
    LivePlayerWeaponState& state,
    const PlayerRuntimeSlot& player,
    std::uint32_t current_tick,
    LivePlayerWeaponStepResult& result) {
    if (state.pending_air_release_spawns <= 0 || !state.next_air_release_spawn_tick ||
        !present(state.pending_air_release_spawn_id)) return;

    const auto interval = static_cast<std::uint32_t>(
        std::max(1, slot.powerup_air_time_between_release_spawns));
    while (state.pending_air_release_spawns > 0 &&
           static_cast<std::int32_t>(current_tick - *state.next_air_release_spawn_tick) >= 0) {
        result.powerup_requests.push_back(
            build_powerup_request(state.pending_air_release_spawn_id, player));
        --state.pending_air_release_spawns;
        *state.next_air_release_spawn_tick += interval;
    }
    if (state.pending_air_release_spawns <= 0) {
        state.next_air_release_spawn_tick.reset();
        state.pending_air_release_spawn_id = FourCC{};
    }
}

} // namespace

LivePlayerWeaponCatalog compile_live_player_weapon_catalog(
    const GameDefinitions& definitions) {
    LivePlayerWeaponCatalog out;
    const auto air_type = fourcc('P','E','A','A');
    const auto ground_type = fourcc('P','E','A','G');
    const auto default_air = fourcc('D','E','A','A');
    const auto default_ground = fourcc('D','E','A','G');

    for (const auto& tagged : definitions.weapons()) {
        const auto& def = tagged.definition;
        LivePlayerWeaponSlot slot;
        slot.id = tagged.id;
        slot.name = def.name;
        slot.type = def.fields.id_value("type_ID").value_or(FourCC{});
        slot.default_marker = def.fields.id_value("default_ID").value_or(FourCC{});
        slot.minimum_level_available = def.fields.int_value("minimumLevelAvailable_INT").value_or(0);
        slot.maximum_level_available = def.fields.int_value("maximumLevelAvailable_INT").value_or(9999);
        slot.auto_repeat = def.fields.bool_value("autoRepeat_BOOL").value_or(false);
        slot.delay_between_launches = def.fields.int_value("delayBetweenLaunches_INT").value_or(0);
        slot.powerup_air_time_until_activation =
            def.fields.int_value("powerup_Air_TimeUntilActivation_INT").value_or(0);
        slot.powerup_air_activation_spawn_id =
            def.fields.id_value("powerup_Air_ActivationSpawn_ID").value_or(FourCC{});
        slot.powerup_air_time_between_power_level_changes =
            def.fields.int_value("powerup_Air_TimeBetweenPowerLevelChanges_INT").value_or(0);
        slot.powerup_air_max_power_level =
            def.fields.int_value("powerup_Air_MaxPowerLevel_INT").value_or(0);
        slot.powerup_air_overload_time =
            def.fields.int_value("powerup_Air_OverloadTime_INT").value_or(0);
        slot.powerup_air_release_spawn_id =
            def.fields.id_value("powerup_Air_ReleaseSpawn_ID").value_or(FourCC{});
        slot.powerup_air_time_between_release_spawns =
            def.fields.int_value("powerup_Air_TimeBetweenReleaseSpawns_INT").value_or(0);
        slot.powerup_air_do_release_on_max_power_level =
            def.fields.bool_value("powerup_Air_DoReleaseOnMaxPowerLevel_BOOL").value_or(false);
        slot.player1_appearance_face = def.fields.id_value("player1AppearanceFace_ID").value_or(FourCC{});
        slot.player2_appearance_face = def.fields.id_value("player2AppearanceFace_ID").value_or(FourCC{});
        slot.score_bar_preview_face = def.fields.id_value("scoreBarPreviewFace_ID").value_or(FourCC{});
        slot.score_bar_preview_frame = def.fields.int_value("scoreBarPreviewFrame_INT").value_or(0);
        slot.spawns = def.spawns;

        if (slot.type == air_type) {
            if (slot.default_marker == default_air) out.default_air = out.air.size();
            out.air.push_back(std::move(slot));
        } else if (slot.type == ground_type) {
            if (slot.default_marker == default_ground) out.default_ground = out.ground.size();
            out.ground.push_back(std::move(slot));
        }
    }
    return out;
}

void initialize_live_player_weapon_state(
    LivePlayerWeaponState& state,
    const LivePlayerWeaponCatalog& catalog,
    int level_number) {
    state = {};
    state.selected_air = first_available(catalog.air, catalog.default_air, level_number);
    state.selected_ground = first_available(catalog.ground, catalog.default_ground, level_number);
}

const LivePlayerWeaponSlot* selected_live_air_weapon(
    const LivePlayerWeaponCatalog& catalog,
    const LivePlayerWeaponState& state) noexcept {
    return state.selected_air < catalog.air.size() ? &catalog.air[state.selected_air] : nullptr;
}

const LivePlayerWeaponSlot* selected_live_ground_weapon(
    const LivePlayerWeaponCatalog& catalog,
    const LivePlayerWeaponState& state) noexcept {
    return state.selected_ground < catalog.ground.size() ? &catalog.ground[state.selected_ground] : nullptr;
}

LivePlayerWeaponStepResult advance_live_player_weapons(
    const LivePlayerWeaponCatalog& catalog,
    LivePlayerWeaponState& state,
    const LivePlayerWeaponInput& input,
    const PlayerRuntimeSlot& player,
    std::uint32_t current_tick,
    int level_number) {
    LivePlayerWeaponStepResult result;

    if (input.switch_air && !state.switch_was_down && !catalog.air.empty()) {
        const auto next = next_available(catalog.air, state.selected_air, level_number);
        result.air_switched = next != state.selected_air;
        if (result.air_switched) {
            clear_powerup_hold(state);
            state.pending_air_release_spawns = 0;
            state.next_air_release_spawn_tick.reset();
            state.pending_air_release_spawn_id = FourCC{};
        }
        state.selected_air = next;
    }

    if (const auto* air = selected_live_air_weapon(catalog, state)) {
        if (available(*air, level_number)) {
            // Any already-scheduled charged release stream executes before new
            // hold logic so its cadence remains stable even if the player taps
            // the weapon again while the previous burst is still unwinding.
            emit_due_powerup_release_spawns(*air, state, player, current_tick, result);

            const bool rising = input.fire_air && !state.air_was_down;
            const bool falling = !input.fire_air && state.air_was_down;

            if (rising) begin_powerup_hold(state, current_tick);

            if (launch_due(
                    *air, input.fire_air, state.air_was_down,
                    state.last_air_launch_tick, current_tick)) {
                result.air_launched = true;
                result.air_launch = build_launch(*air, player, false);
                state.last_air_launch_tick = current_tick;
            }

            if (has_air_powerup(*air) && input.fire_air && state.air_hold_started_tick) {
                const auto held_ticks = current_tick - *state.air_hold_started_tick;
                const auto activate_at = static_cast<std::uint32_t>(
                    std::max(0, air->powerup_air_time_until_activation));
                if (!state.air_powerup_activated_tick && held_ticks >= activate_at) {
                    state.air_powerup_activated_tick = current_tick;
                    result.air_powerup_activated = true;
                    result.powerup_requests.push_back(
                        build_powerup_request(air->powerup_air_activation_spawn_id, player));
                }

                if (state.air_powerup_activated_tick) {
                    const auto interval = static_cast<std::uint32_t>(
                        std::max(1, air->powerup_air_time_between_power_level_changes));
                    const auto charging_ticks = current_tick - *state.air_powerup_activated_tick;
                    state.air_power_level = std::min(
                        air->powerup_air_max_power_level,
                        static_cast<int>(charging_ticks / interval));

                    const bool max_release =
                        air->powerup_air_do_release_on_max_power_level &&
                        state.air_power_level >= air->powerup_air_max_power_level;
                    const bool overload_release =
                        air->powerup_air_overload_time > 0 &&
                        charging_ticks >= static_cast<std::uint32_t>(air->powerup_air_overload_time);
                    if (max_release || overload_release) {
                        result.air_powerup_overloaded = overload_release;
                        schedule_powerup_release(*air, state, current_tick, result);
                        emit_due_powerup_release_spawns(*air, state, player, current_tick, result);
                    }
                }
            }

            if (falling && state.air_powerup_activated_tick) {
                schedule_powerup_release(*air, state, current_tick, result);
                emit_due_powerup_release_spawns(*air, state, player, current_tick, result);
            } else if (falling) {
                clear_powerup_hold(state);
            }

            result.air_power_level = state.air_power_level;
            result.air_power_percentage = power_percentage(*air, state.air_power_level);
        }
    }

    if (const auto* ground = selected_live_ground_weapon(catalog, state)) {
        if (available(*ground, level_number) && launch_due(
                *ground, input.fire_ground, state.ground_was_down,
                state.last_ground_launch_tick, current_tick)) {
            result.ground_launched = true;
            result.ground_launch = build_launch(*ground, player, true);
            state.last_ground_launch_tick = current_tick;
        }
    }

    state.air_was_down = input.fire_air;
    state.ground_was_down = input.fire_ground;
    state.switch_was_down = input.switch_air;
    return result;
}

} // namespace deimos
