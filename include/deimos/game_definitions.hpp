#pragma once

#include "deimos/pak_archive.hpp"
#include "deimos/player_definition.hpp"
#include "deimos/unit_definition.hpp"
#include "deimos/weapon_definition.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace deimos {

template <class T>
struct TaggedDefinition {
    FourCC id{};
    std::string path;
    T definition;
};

struct DefinitionReferenceIssue {
    std::string source_path;
    std::string field;
    FourCC target{};
};

class GameDefinitions {
public:
    static std::optional<GameDefinitions> load_from_game_pak(
        const PakArchive& pak,
        std::string* error = nullptr);

    [[nodiscard]] const std::vector<TaggedDefinition<UnitDefinition>>& units() const { return units_; }
    [[nodiscard]] const std::vector<TaggedDefinition<WeaponDefinition>>& weapons() const { return weapons_; }
    [[nodiscard]] const std::vector<TaggedDefinition<PlayerDefinition>>& players() const { return players_; }

    [[nodiscard]] const UnitDefinition* find_unit(FourCC id) const;
    [[nodiscard]] const WeaponDefinition* find_weapon(FourCC id) const;
    [[nodiscard]] const PlayerDefinition* find_player(FourCC id) const;

    // Validates only fields proven by names/corpus usage to reference Unit
    // Definitions. Sprite/sound/particle/list IDs live in other namespaces.
    [[nodiscard]] std::vector<DefinitionReferenceIssue> validate_unit_references() const;

private:
    std::vector<TaggedDefinition<UnitDefinition>> units_;
    std::vector<TaggedDefinition<WeaponDefinition>> weapons_;
    std::vector<TaggedDefinition<PlayerDefinition>> players_;
};

} // namespace deimos
