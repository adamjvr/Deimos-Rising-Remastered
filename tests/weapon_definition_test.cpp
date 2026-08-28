#include "deimos/weapon_definition.hpp"

#include <cassert>
#include <string>

int main() {
    const std::string text = R"(#type_ID <PEAA>
#name_STR <Test Weapon>
#autoRepeat_BOOL <TRUE>
#spawn_NumUnitsToSpawn_INT <1>
    #spawn_Name_STR <Left>
    #spawn_Unit_ID <shot>
    #spawn_XLoc_INT <-5>
    #spawn_YLoc_INT <0>
    #spawn_SetHeading_BOOL <TRUE>
    #spawn_Angle_INT <9>
#powerup_Air_MaxPowerLevel_INT <20>
)";
    std::string error;
    auto doc = deimos::parse_tagged_text(text, &error);
    assert(doc);
    auto weapon = deimos::parse_weapon_definition_document(*doc, &error);
    assert(weapon);
    assert(weapon->name == "Test Weapon");
    assert(weapon->spawns.size() == 1);
    assert(weapon->spawns[0].unit_id.str() == "shot");
    assert(weapon->spawns[0].angle_degrees == 9);
    assert(weapon->fields.bool_value("autoRepeat_BOOL") == true);
}
