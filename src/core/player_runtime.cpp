#include "deimos/player_runtime.hpp"

#include <algorithm>
#include <array>
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

    // 0x28114..0x28124 raises +0xCE when the player instance is enabled. A
    // collision-reachable active player is enabled in the clean model.
    player.invulnerable = true;
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
    out.life_spawn = definition.fields.id_value("life_Spawn_ID").value_or(FourCC{});
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

} // namespace deimos
