#include "deimos/unit_definition.hpp"

#include <cassert>
#include <string>

int main() {
    const std::string text = R"(#name_STR <Synthetic Unit>
#familyName_STR <Test>
#description_STR <Synthetic fixture>
#damage_FLOAT <2.500000>
#numStates_INT <1>
#stateName_STR <Wait For Player Approach>
#stateOnTimerMin_INT <10>
#stateOnTimerMax_INT <20>
#stateOnTimerChangeTo_STR <Delete>
#stateNumSpawnSets_INT <1>
    #stateSpawnSetName_STR <Shot>
    #stateSpawnSetSpawn_ID <shot>
    #stateSpawnSetXOffset_INT <2>
    #stateSpawnSetYOffset_INT <-3>
    #stateSpawnSetAdjustOffsetForUnitRotation_BOOL <TRUE>
    #stateSpawnSet_AbsoluteCoordinates_BOOL <FALSE>
    #stateSpawnSetRateMin_INT <4>
    #stateSpawnSetRateMax_INT <6>
    #stateSpawnSetNumInVolleyMin_INT <1>
    #stateSpawnSetNumInVolleyMax_INT <2>
    #stateSpawnSetDelayBetweenEntitiesMin_INT <0>
    #stateSpawnSetDelayBetweenEntitiesMax_INT <1>
    #stateSpawnSetRepeatSpawns_BOOL <TRUE>
    #stateSpawnSetDon'tSpawnOffscreen_BOOL <TRUE>
    #stateSpawnSetPauseAnyRotationWhileSpawning_BOOL <FALSE>
    #stateSpawnSetTimeToPauseRotationAfterSpawning_INT <0>
    #stateSpawnSetSpawnIfFleeing_BOOL <FALSE>
    #stateSpawnSet_StationaryOption_BOOL <FALSE>
    #stateSpawnSet_TerrainEffectsOption_BOOL <FALSE>
    #stateSpawnSetSetHeading_BOOL <TRUE>
    #stateSpawnSetHeadingDegrees_INT <90>
#stateHunts_BOOL <TRUE>
#stateNumRules_INT <1>
    #stateRuleName_STR <Rule>
    #stateRuleUnit_ID <none>
    #stateRuleRange_INT <100>
    #stateRuleCondition_STR <Is Tracking Player>
    #stateRuleAction_STR <Wait for Player Approach>
)";

    std::string error;
    auto doc = deimos::parse_tagged_text(text, &error);
    assert(doc);
    auto unit = deimos::parse_unit_definition_document(*doc, &error);
    assert(unit);
    assert(unit->name == "Synthetic Unit");
    assert(unit->family_name == "Test");
    assert(unit->core_fields.float_value("damage_FLOAT") == 2.5f);
    assert(unit->states.size() == 1);
    assert(unit->states[0].spawn_sets.size() == 1);
    assert(unit->states[0].rules.size() == 1);
    assert(unit->states[0].spawn_sets[0].spawn_id.str() == "shot");
    assert(unit->states[0].spawn_sets[0].heading_degrees == 90);
    assert(unit->states[0].fields.bool_value("stateHunts_BOOL") == true);
    // PPC 0x57820 is exact strcmp: case-only mismatches do not resolve.
    assert(unit->find_state("Wait For Player Approach") == 0);
    assert(!unit->find_state("Wait for Player Approach"));

    // Binary-confirmed 1.0.6 invariant: unit definitions support at most 20 states.
    std::string invalid = R"(#name_STR <Too Many States>
#familyName_STR <Test>
#description_STR <Synthetic fixture>
#numStates_INT <21>
)";
    error.clear();
    auto invalid_doc = deimos::parse_tagged_text(invalid, &error);
    assert(invalid_doc);
    auto invalid_unit = deimos::parse_unit_definition_document(*invalid_doc, &error);
    assert(!invalid_unit);
    assert(error.find("1..20") != std::string::npos);

}

