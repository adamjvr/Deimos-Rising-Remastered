#include "deimos/game_definitions.hpp"

#include "deimos/resource_id.hpp"

#include <array>

namespace deimos {
namespace {
void fail(std::string* error, std::string message) { if (error) *error = std::move(message); }

bool sentinel(FourCC id) {
    return id.str() == "none" || id.str() == "NULL";
}

template <class T>
const T* find_tagged(const std::vector<TaggedDefinition<T>>& values, FourCC id) {
    for (const auto& value : values) if (value.id == id) return &value.definition;
    return nullptr;
}

template <class T>
bool duplicate_id(const std::vector<TaggedDefinition<T>>& values, FourCC id) {
    for (const auto& value : values) if (value.id == id) return true;
    return false;
}

} // namespace

std::optional<GameDefinitions> GameDefinitions::load_from_game_pak(
    const PakArchive& pak, std::string* error) {
    GameDefinitions out;
    for (const auto& entry : pak.entries()) {
        if (entry.is_directory) continue;
        const auto parsed_name = parse_resource_name(entry.path);
        if (!parsed_name) continue;
        if (parsed_name->kind != ResourceKind::unit &&
            parsed_name->kind != ResourceKind::weapon &&
            parsed_name->kind != ResourceKind::player) continue;

        auto bytes = pak.read(entry, error);
        if (!bytes) return std::nullopt;

        if (parsed_name->kind == ResourceKind::unit) {
            if (duplicate_id(out.units_, parsed_name->tag)) {
                fail(error, "duplicate Unit Definition ID " + parsed_name->tag.str());
                return std::nullopt;
            }
            auto definition = decode_and_parse_unit_definition(*bytes, error);
            if (!definition) { if (error) *error = entry.path + ": " + *error; return std::nullopt; }
            out.units_.push_back({parsed_name->tag, entry.path, std::move(*definition)});
        } else if (parsed_name->kind == ResourceKind::weapon) {
            if (duplicate_id(out.weapons_, parsed_name->tag)) {
                fail(error, "duplicate Weapon Definition ID " + parsed_name->tag.str());
                return std::nullopt;
            }
            auto definition = decode_and_parse_weapon_definition(*bytes, error);
            if (!definition) { if (error) *error = entry.path + ": " + *error; return std::nullopt; }
            out.weapons_.push_back({parsed_name->tag, entry.path, std::move(*definition)});
        } else {
            if (duplicate_id(out.players_, parsed_name->tag)) {
                fail(error, "duplicate Player Definition ID " + parsed_name->tag.str());
                return std::nullopt;
            }
            auto definition = decode_and_parse_player_definition(*bytes, error);
            if (!definition) { if (error) *error = entry.path + ": " + *error; return std::nullopt; }
            out.players_.push_back({parsed_name->tag, entry.path, std::move(*definition)});
        }
    }
    return out;
}

const UnitDefinition* GameDefinitions::find_unit(FourCC id) const { return find_tagged(units_, id); }
const WeaponDefinition* GameDefinitions::find_weapon(FourCC id) const { return find_tagged(weapons_, id); }
const PlayerDefinition* GameDefinitions::find_player(FourCC id) const { return find_tagged(players_, id); }

std::vector<DefinitionReferenceIssue> GameDefinitions::validate_unit_references() const {
    std::vector<DefinitionReferenceIssue> issues;
    const auto check = [&](std::string_view path, std::string_view field, FourCC id) {
        if (!sentinel(id) && !find_unit(id)) issues.push_back({std::string(path), std::string(field), id});
    };

    static constexpr std::array<std::string_view, 5> unit_core_refs = {
        "pickup_MultiplierSpawn_ID", "destructSpawn_ID", "destructCoin_ID",
        "destructCoinOnGroupKill_ID", "deletionSpawn_ID"
    };
    for (const auto& tagged : units_) {
        for (const auto key : unit_core_refs) {
            if (const auto id = tagged.definition.core_fields.id_value(key)) check(tagged.path, key, *id);
        }
        for (const auto& state : tagged.definition.states) {
            if (const auto id = state.fields.id_value("collision_Spawn_ID")) check(tagged.path, "collision_Spawn_ID", *id);
            for (const auto& spawn : state.spawn_sets) check(tagged.path, "stateSpawnSetSpawn_ID", spawn.spawn_id);
            for (const auto& rule : state.rules) check(tagged.path, "stateRuleUnit_ID", rule.unit_id);
        }
    }

    static constexpr std::array<std::string_view, 5> weapon_refs = {
        "crosshairSpawnOnActivation_ID", "powerup_Air_ActivationSpawn_ID",
        "powerup_Air_ReleaseSpawn_ID", "powerup_Ground_ActivationSpawn_ID",
        "powerup_Ground_ReleaseSpawn_ID"
    };
    for (const auto& tagged : weapons_) {
        for (const auto key : weapon_refs) {
            if (const auto id = tagged.definition.fields.id_value(key)) check(tagged.path, key, *id);
        }
        for (const auto& spawn : tagged.definition.spawns) check(tagged.path, "spawn_Unit_ID", spawn.unit_id);
    }

    static constexpr std::array<std::string_view, 7> player_refs = {
        "life_Spawn_ID", "entry_Spawn_ID", "death_Spawn_ID", "active_MoneyCounterSpawn_ID",
        "active_SpawnOnHit_ID", "active_ShieldWarningObject_ID", "active_DefenceBonusObject_ID"
    };
    for (const auto& tagged : players_) {
        for (const auto key : player_refs) {
            if (const auto id = tagged.definition.fields.id_value(key)) check(tagged.path, key, *id);
        }
    }
    return issues;
}

} // namespace deimos
