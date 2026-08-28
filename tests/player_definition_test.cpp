#include "deimos/player_definition.hpp"

#include <cassert>
#include <string>

int main() {
    const std::string text = R"(#name_STR <Player 1>
#defaultShieldPercentage_INT <100.000000>
#active_DefaultMaxSpeed_FLOAT <7.800000>
#entry_Spawn_ID <plen>
)";
    std::string error;
    auto doc = deimos::parse_tagged_text(text, &error);
    assert(doc);
    auto player = deimos::parse_player_definition_document(*doc, &error);
    assert(player);
    assert(player->name == "Player 1");
    assert(player->fields.int_value("defaultShieldPercentage_INT") == 100);
    assert(player->fields.float_value("active_DefaultMaxSpeed_FLOAT") == 7.8f);
    assert(player->fields.id_value("entry_Spawn_ID")->str() == "plen");
}
