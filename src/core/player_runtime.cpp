#include "deimos/player_runtime.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <string>
#include <string_view>

namespace deimos {
namespace {

constexpr FourCC fourcc(char a, char b, char c, char d) {
    return FourCC{{a, b, c, d}};
}

bool none_or_empty(FourCC id) {
    return id == FourCC{} || id.str() == "none" || id.str() == "NULL";
}

std::optional<FourCC> present(FourCC id) {
    if (none_or_empty(id)) return std::nullopt;
    return id;
}

bool legacy_tick_strictly_after(
    std::uint32_t current_tick,
    std::uint32_t since_tick,
    int duration_ticks) {
    // 0x2A150 uses a 32-bit `add` followed by signed `cmpw`, then takes the
    // transition only when currentTick > deadline. Preserve the wrap/sign
    // behavior rather than widening the arithmetic.
    const auto deadline_bits = since_tick + static_cast<std::uint32_t>(duration_ticks);
    return std::bit_cast<std::int32_t>(current_tick) >
        std::bit_cast<std::int32_t>(deadline_bits);
}

void enter_active_player_state(
    PlayerRuntimeSlot& player,
    const CompiledPlayerRuntimeDefinition& definition,
    std::uint32_t current_tick,
    LegacyPlayerLifecycleResult& result) {
    // 0x29D00..0x29DA4: +0xCD chooses the solo or multiplayer coordinate pair.
    const int entry_x = player.use_solo_entry_position
        ? definition.entry_solo_start_x
        : definition.entry_multi_start_x;
    const int entry_y = player.use_solo_entry_position
        ? definition.entry_solo_start_y
        : definition.entry_multi_start_y;
    player.x = static_cast<float>(entry_x);
    player.y = static_cast<float>(entry_y);

    // 0x29DA8..0x29DC4 loads the same canonical zero-vector used elsewhere in
    // the engine. PlayerDef entry_StartVelocityX/Y are not read by this helper.
    player.velocity_x = 0.0f;
    player.velocity_y = 0.0f;

    player.status = static_cast<int>(LegacyPlayerStatus::active);
    player.status_since_tick = current_tick;
    result.respawned = true;
    result.respawn_position = EntityPoint{player.x, player.y};
    result.entry_spawn_due = present(definition.entry_spawn);
}

void enter_legacy_player_death(
    PlayerRuntimeSlot& player,
    const CompiledPlayerRuntimeDefinition& definition,
    std::uint32_t current_tick,
    const LegacyPlayerRuntimeResources* resources,
    LegacyPlayerDamageResult& result) {
    result.death_entered = true;
    result.death_spawn_due = present(definition.death_spawn);
    result.money_before_death = player.money;

    // 0x27FA0..0x280F8 decomposes the held-money counter into the four classic
    // denominations, in descending order, before 0x28100 clears it.
    int remaining = std::max(player.money, 0);
    constexpr std::array<int, 4> denominations = {50, 10, 5, 1};
    const std::array<FourCC, 4> spawn_ids = resources
        ? std::array<FourCC, 4>{resources->money_50, resources->money_10, resources->money_5, resources->money_1}
        : std::array<FourCC, 4>{};
    for (std::size_t i = 0; i < denominations.size(); ++i) {
        const int value = denominations[i];
        const int count = remaining / value;
        if (count > 0) result.money_drops.push_back({value, count, spawn_ids[i]});
        remaining %= value;
    }

    player.money = 0;
    player.status = 3; // exact value written at 0x28108..0x2810C
    player.status_since_tick = current_tick;
    player.last_shield_hit_tick = 0;
    player.last_spawn_on_hit_tick = 0;
    player.shield_warning_latched = false;

    // 0x28114..0x28124 raises +0xCE only while player +0xC4 is enabled.
    if (player.enabled) player.invulnerable = true;
}

} // namespace

std::optional<LegacyPlayerRuntimeGlobals> compile_legacy_player_runtime_globals(
    const NamedTable<float>& game_floats,
    std::string* error) {
    constexpr std::size_t kImpact = 161;
    constexpr std::size_t kSpawnDelay = 162;
    constexpr std::size_t kEntityHitDelay = 167;
    if (game_floats.size() <= kEntityHitDelay) {
        if (error) *error = "Game[gafl] is shorter than the 1.0.6 player-runtime positional contract";
        return std::nullopt;
    }
    constexpr std::array<std::pair<std::size_t, std::string_view>, 3> expected = {{
        {kImpact, "Player_ImpactDamageToEntities"},
        {kSpawnDelay, "Player_DelayBetweenHitSpawns"},
        {kEntityHitDelay, "Entity_HitDelay"},
    }};
    for (const auto& [index, label] : expected) {
        if (game_floats[index].first != label) {
            if (error) {
                *error = "unexpected Game[gafl] player-runtime label at index " +
                    std::to_string(index);
            }
            return std::nullopt;
        }
    }
    return LegacyPlayerRuntimeGlobals{
        game_floats[kImpact].second,
        static_cast<int>(game_floats[kSpawnDelay].second),
        static_cast<int>(game_floats[kEntityHitDelay].second),
    };
}

std::optional<LegacyPlayerScoreGlobals> compile_legacy_player_score_globals(
    const NamedTable<float>& game_floats,
    std::string* error) {
    constexpr std::size_t index = 182;
    if (game_floats.size() <= index) {
        if (error) *error = "Game[gafl] is shorter than Player_ExtraLifeScoreAdjustment";
        return std::nullopt;
    }
    if (game_floats[index].first != "Player_ExtraLifeScoreAdjustment") {
        if (error) *error = "unexpected Game[gafl] player-score label at index 182";
        return std::nullopt;
    }
    return LegacyPlayerScoreGlobals{static_cast<int>(game_floats[index].second)};
}

std::optional<LegacyPlayerRuntimeResources> compile_legacy_player_runtime_resources(
    const NamedTable<FourCC>& game_objects,
    std::string* error) {
    constexpr std::size_t kFirst = 2;
    constexpr std::array<std::string_view, 4> labels = {
        "MoneyUnit_50", "MoneyUnit_10", "MoneyUnit_5", "MoneyUnit_1"
    };
    if (game_objects.size() < kFirst + labels.size()) {
        if (error) *error = "Objects[gaob] is shorter than the 1.0.6 player-money positional contract";
        return std::nullopt;
    }
    for (std::size_t i = 0; i < labels.size(); ++i) {
        if (game_objects[kFirst + i].first != labels[i]) {
            if (error) {
                *error = "unexpected Objects[gaob] player-money label at index " +
                    std::to_string(kFirst + i);
            }
            return std::nullopt;
        }
    }
    return LegacyPlayerRuntimeResources{
        game_objects[2].second, game_objects[3].second,
        game_objects[4].second, game_objects[5].second
    };
}

CompiledPlayerRuntimeDefinition compile_player_runtime_definition(
    const PlayerDefinition& definition) {
    CompiledPlayerRuntimeDefinition out;
    out.score_bar_face = definition.fields.id_value("spriteScoreBar_ID").value_or(FourCC{});
    out.score_bar_frame = definition.fields.int_value("spriteScoreBarFrame_INT").value_or(0);
    out.score_bar_power_face = definition.fields.id_value("spriteScoreBarPower_ID").value_or(FourCC{});
    out.score_bar_power_frame = definition.fields.int_value("spriteScoreBarPowerFrame_INT").value_or(0);
    out.score_bar_shield_face = definition.fields.id_value("spriteScoreBarShield_ID").value_or(FourCC{});
    out.score_bar_shield_frame = definition.fields.int_value("spriteScoreBarShieldFrame_INT").value_or(0);
    out.default_shield_percentage =
        definition.fields.float_value("defaultShieldPercentage_INT").value_or(100.0f);
    out.shield_warning_percentage =
        definition.fields.float_value("shieldWarningPercentage_INT").value_or(15.0f);
    out.shield_base_hit_percentage =
        definition.fields.float_value("shieldBaseHitPercentage_INT").value_or(15.0f);
    out.shield_hit_delay_ticks =
        definition.fields.int_value("shieldHitDelay_INT").value_or(1);
    out.life_max = definition.fields.int_value("life_MaxNum_INT").value_or(10);
    out.life_initial = definition.fields.int_value("life_NumInitial_INT").value_or(3);
    out.life_initial_required_score =
        definition.fields.int_value("life_InitialRequiredScore_INT").value_or(10000);
    out.life_additional_required_score =
        definition.fields.int_value("life_AdditionalRequiredScore_INT").value_or(30000);
    out.life_spawn = definition.fields.id_value("life_Spawn_ID").value_or(FourCC{});
    out.game_over_time_ticks =
        definition.fields.int_value("gameOverTime_INT").value_or(20);
    out.dying_time_ticks =
        definition.fields.int_value("dyingTime_INT").value_or(80);
    out.final_dying_time_ticks =
        definition.fields.int_value("finalDyingTime_INT").value_or(40);
    out.entry_invulnerability_time_ticks =
        definition.fields.int_value("entry_InvulnerabilityTime_INT").value_or(60);
    out.entry_solo_start_x =
        definition.fields.int_value("entry_soloStartX_INT").value_or(208);
    out.entry_solo_start_y =
        definition.fields.int_value("entry_soloStartY_INT").value_or(330);
    out.entry_multi_start_x =
        definition.fields.int_value("entry_multiStartX_INT").value_or(104);
    out.entry_multi_start_y =
        definition.fields.int_value("entry_multiStartY_INT").value_or(330);
    out.entry_spawn = definition.fields.id_value("entry_Spawn_ID").value_or(FourCC{});
    out.entry_initial_delay_ticks =
        definition.fields.int_value("entry_InitialDelay_INT").value_or(55);
    out.death_spawn = definition.fields.id_value("death_Spawn_ID").value_or(FourCC{});
    out.active_spawn_on_hit =
        definition.fields.id_value("active_SpawnOnHit_ID").value_or(FourCC{});
    out.active_shield_warning_object =
        definition.fields.id_value("active_ShieldWarningObject_ID").value_or(FourCC{});
    out.active_defence_bonus_object =
        definition.fields.id_value("active_DefenceBonusObject_ID").value_or(FourCC{});
    return out;
}

void initialize_legacy_player_gameplay(
    PlayerRuntimeSlot& player,
    const CompiledPlayerRuntimeDefinition& definition) {
    player.shield_percentage = definition.default_shield_percentage;
    player.lives = definition.life_initial;
    player.money = 0;
    player.power_multiplier = 1;
    player.invulnerable = false;
    player.invulnerability_latched = false;
    player.shield_warning_latched = false;
    player.shield_hit_latched = false;
    player.last_shield_hit_tick = 0;
    player.last_spawn_on_hit_tick = 0;
    player.status_since_tick = 0;
    player.score = 0;
    player.next_extra_life_score = definition.life_initial_required_score;
    player.extra_life_score_adjustment = 0;
}

LegacyPlayerScoreResult apply_legacy_player_score(
    PlayerRuntimeSlot& player,
    const CompiledPlayerRuntimeDefinition& definition,
    const LegacyPlayerScoreGlobals& globals,
    int points,
    bool raw_score_mode) {
    LegacyPlayerScoreResult out;
    out.applied = player.enabled;
    out.raw_score_mode = raw_score_mode;
    out.requested_points = points;
    out.multiplier = raw_score_mode ? 1 : player.power_multiplier;
    out.awarded_points = raw_score_mode ? points : points * player.power_multiplier;
    out.score_before = player.score;
    out.score_after = player.score;
    out.lives_before = player.lives;
    out.lives_after = player.lives;
    out.next_threshold_before = player.next_extra_life_score;
    out.next_threshold_after = player.next_extra_life_score;
    out.adjustment_before = player.extra_life_score_adjustment;
    out.adjustment_after = player.extra_life_score_adjustment;
    if (!player.enabled) return out;

    player.score += out.awarded_points;
    out.score_after = player.score;

    if (points > 0) {
        if (raw_score_mode) {
            player.extra_life_score_adjustment =
                player.score + globals.extra_life_score_adjustment;
        } else if (player.score > player.next_extra_life_score) {
            out.extra_life_threshold_crossed = true;
            if (player.lives < definition.life_max) {
                ++player.lives;
                if (!(definition.life_spawn == FourCC{})) out.life_spawn_due = definition.life_spawn;
            }
            player.next_extra_life_score +=
                definition.life_additional_required_score + player.extra_life_score_adjustment;
            player.extra_life_score_adjustment += globals.extra_life_score_adjustment;
        }
    }

    out.lives_after = player.lives;
    out.next_threshold_after = player.next_extra_life_score;
    out.adjustment_after = player.extra_life_score_adjustment;
    return out;
}

LegacyPlayerPickupResult apply_legacy_player_pickup(
    PlayerRuntimeSlot& player,
    const EntityRuntime& pickup,
    const CompiledPlayerRuntimeDefinition& definition) {
    LegacyPlayerPickupResult out;
    out.pickup_type = pickup.behavior.pickup_type;
    out.pickup_value = pickup.behavior.pickup_value;
    out.money_before = player.money;
    out.money_after = player.money;
    out.lives_before = player.lives;
    out.lives_after = player.lives;
    out.multiplier_before = player.power_multiplier;
    out.multiplier_after = player.power_multiplier;
    out.shield_before = player.shield_percentage;
    out.shield_after = player.shield_percentage;

    const FourCC type = pickup.behavior.pickup_type;

    if (type == fourcc('a','i','r',' ') || type == fourcc('g','r','n','d')) {
        // 0x37630 / 0x37648 call the +0xCE getter. True means reject.
        out.accepted = !player.invulnerable;
        return out;
    }

    if (type == fourcc('c','o','i','n')) {
        if (pickup.behavior.pickup_value != 0) {
            player.money += pickup.behavior.pickup_value;
            out.feedback_due = true; // 0x12BC0(...,32767,6,0)
        }
    } else if (type == fourcc('m','u','l','t')) {
        // Jump table at r2+0x3090: 1->2->3->4->5->10, all other values no-op.
        switch (player.power_multiplier) {
            case 1: player.power_multiplier = 2; break;
            case 2: player.power_multiplier = 3; break;
            case 3: player.power_multiplier = 4; break;
            case 4: player.power_multiplier = 5; break;
            case 5: player.power_multiplier = 10; break;
            default: break;
        }
    } else if (type == fourcc('e','x','l','i')) {
        if (player.lives < definition.life_max) {
            ++player.lives;
            out.spawn_due = present(definition.life_spawn);
        }
    } else if (type == fourcc('s','h','i','e')) {
        // 0x27490 clamps pickup additions to [0,100].
        player.shield_percentage = std::clamp(
            player.shield_percentage + static_cast<float>(pickup.behavior.pickup_value),
            0.0f, 100.0f);
    }
    // 'spec' and every other legacy value are accepted no-ops in 0x37580.

    out.money_after = player.money;
    out.lives_after = player.lives;
    out.multiplier_after = player.power_multiplier;
    out.shield_after = player.shield_percentage;
    return out;
}

LegacyPlayerDamageResult apply_legacy_player_damage(
    PlayerRuntimeSlot& player,
    const CompiledPlayerRuntimeDefinition& definition,
    float damage,
    std::uint32_t current_tick,
    int delay_between_hit_spawns,
    const LegacyPlayerRuntimeResources* resources) {
    LegacyPlayerDamageResult out;
    out.requested_damage = damage;
    out.shield_before = player.shield_percentage;
    out.shield_after = player.shield_percentage;

    // 0x2712C: only active status 4 takes this path.
    if (player.status != 4) return out;

    // 0x27138..0x27150 uses signed integer tick arithmetic and stores the new
    // hit tick before testing invulnerability.
    const auto hit_deadline = static_cast<std::int64_t>(player.last_shield_hit_tick) +
        definition.shield_hit_delay_ticks;
    if (static_cast<std::int64_t>(current_tick) < hit_deadline) {
        out.blocked_by_hit_delay = true;
        return out;
    }
    player.last_shield_hit_tick = current_tick;
    out.processed = true;

    if (player.invulnerable) {
        out.invulnerability_bypassed_shield_damage = true;
    } else {
        out.scaled_shield_damage = damage * definition.shield_base_hit_percentage;
        player.shield_percentage -= out.scaled_shield_damage;
        if (out.scaled_shield_damage > 0.0f) player.shield_hit_latched = true;
    }
    out.shield_after = player.shield_percentage;

    // Strictly negative dies; exactly zero remains active (0x271C8..0x271E8).
    if (player.shield_percentage < 0.0f) {
        enter_legacy_player_death(player, definition, current_tick, resources, out);
        return out;
    }

    // The hit-glow call is after the invulnerability branch and therefore also
    // occurs for invulnerable hits that pass the tick gate.
    out.hit_glow_due = true;

    if (damage <= 0.0f) return out;

    if (const auto spawn = present(definition.active_spawn_on_hit)) {
        const auto spawn_deadline = static_cast<std::int64_t>(player.last_spawn_on_hit_tick) +
            delay_between_hit_spawns;
        if (static_cast<std::int64_t>(current_tick) >= spawn_deadline) {
            player.last_spawn_on_hit_tick = current_tick;
            out.spawn_on_hit_due = spawn;
        }
    }

    if (!player.shield_warning_latched &&
        player.shield_percentage <= definition.shield_warning_percentage) {
        out.shield_warning_due = present(definition.active_shield_warning_object);
        // 0x273D0 sets the latch even when the resource itself is 'none'.
        player.shield_warning_latched = true;
    }

    return out;
}

LegacyPlayerLifecycleResult advance_legacy_player_lifecycle(
    PlayerRuntimeSlot& player,
    const CompiledPlayerRuntimeDefinition& definition,
    std::uint32_t current_tick,
    bool consume_life_on_death,
    bool defer_invulnerability_expiry) {
    LegacyPlayerLifecycleResult out;
    out.status_before = player.status;
    out.status_after = player.status;

    // 0x2A17C: disabled players skip the entire lifecycle switch.
    if (!player.enabled) return out;

    switch (player.status) {
        case static_cast<int>(LegacyPlayerStatus::game_over): {
            // 0x2A244..0x2A268: game-over remains enabled through equality and
            // only disables the player when currentTick is strictly later.
            if (legacy_tick_strictly_after(
                    current_tick, player.status_since_tick,
                    definition.game_over_time_ticks)) {
                player.enabled = false;
                out.disabled_after_game_over = true;
            }
            break;
        }

        case static_cast<int>(LegacyPlayerStatus::waiting): {
            // 0x2A1B4..0x2A1E4: before the strict entry deadline the original
            // only drives a zero-valued external player channel. The headless
            // core records that bounded side effect without inventing it.
            if (legacy_tick_strictly_after(
                    current_tick, player.status_since_tick,
                    definition.entry_initial_delay_ticks)) {
                enter_active_player_state(player, definition, current_tick, out);
            } else {
                out.active_entry_waiting = true;
            }
            break;
        }

        case static_cast<int>(LegacyPlayerStatus::dying): {
            // 0x2A290..0x2A2B8: the final remaining life uses the distinct
            // finalDyingTime; every other semantic life count uses dyingTime.
            const int duration = player.lives == 1
                ? definition.final_dying_time_ticks
                : definition.dying_time_ticks;
            if (!legacy_tick_strictly_after(
                    current_tick, player.status_since_tick, duration)) {
                break;
            }

            // 0x2A2C8..0x2A2F0: the caller-provided byte controls whether the
            // expired death actually consumes one life. Clamp at zero.
            if (consume_life_on_death && player.enabled) {
                player.lives = std::max(player.lives - 1, 0);
                out.life_decremented = true;
            }

            if (player.lives > 0) {
                enter_active_player_state(player, definition, current_tick, out);

                // 0x2A310..0x2A368 occurs after 0x29CC0 on a death respawn.
                // Enabled players recover the PlayerDef default shield; the
                // original live value is biased, but clean state is semantic.
                player.shield_percentage = player.enabled
                    ? definition.default_shield_percentage
                    : 0.0f;
                player.last_shield_hit_tick = 0;
                player.last_spawn_on_hit_tick = 0;
                player.shield_warning_latched = false;
            } else {
                // 0x2A370..0x2A378: zero lives enters status 1. Player +0xC4
                // remains enabled until gameOverTime expires on a later tick.
                player.status = static_cast<int>(LegacyPlayerStatus::game_over);
                player.status_since_tick = current_tick;
                out.game_over_entered = true;
            }
            break;
        }

        case static_cast<int>(LegacyPlayerStatus::active): {
            // 0x2A1E8..0x2A240: entry/death invulnerability expires only after
            // the strict +0x8C duration, unless 0x5CF0 or +0xCF blocks clear.
            if (player.invulnerable && !defer_invulnerability_expiry &&
                legacy_tick_strictly_after(
                    current_tick, player.status_since_tick,
                    definition.entry_invulnerability_time_ticks) &&
                player.enabled && !player.invulnerability_latched) {
                player.invulnerable = false;
                player.invulnerability_latched = false;
                out.invulnerability_cleared = true;
            }
            break;
        }

        default:
            // Status 0 and >=5 fall through the original switch unchanged.
            break;
    }

    out.status_after = player.status;
    return out;
}

} // namespace deimos
