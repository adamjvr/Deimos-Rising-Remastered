#include "deimos/score_bar_runtime.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <string>

namespace {
bool nearly(float a, float b) { return std::fabs(a - b) < 0.0001f; }
deimos::FourCC id(const char* s) { return deimos::FourCC{{s[0],s[1],s[2],s[3]}}; }
}

int main() {
    deimos::NamedTable<float> game(144, {"unused", 0.0f});
    const char* labels[] = {
        "ScoreBar_ScoreSpacing",
        "ScoreBar_P1LivesSymbol_XLoc", "ScoreBar_P1LivesSymbol_YLoc",
        "ScoreBar_P2LivesSymbol_XLoc", "ScoreBar_P2LivesSymbol_YLoc",
        "ScoreBar_P1ShieldMeter_XLoc", "ScoreBar_P1ShieldMeter_YLoc",
        "ScoreBar_P2ShieldMeter_XLoc", "ScoreBar_P2ShieldMeter_YLoc",
        "ScoreBar_ShieldMeterIncreaseRate", "ScoreBar_ShieldMeterDecreaseRate",
        "ScoreBar_P1PowerMeter_XLoc", "ScoreBar_P1PowerMeter_YLoc",
        "ScoreBar_P2PowerMeter_XLoc", "ScoreBar_P2PowerMeter_YLoc",
        "ScoreBar_PowerMeterIncreaseRate", "ScoreBar_PowerMeterDecreaseRate",
        "ScoreBar_P1Weapons_1_XLoc", "ScoreBar_P1Weapons_1_YLoc",
        "ScoreBar_P1Weapons_2_XLoc", "ScoreBar_P1Weapons_2_YLoc",
        "ScoreBar_P1Weapons_3_XLoc", "ScoreBar_P1Weapons_3_YLoc",
        "ScoreBar_P2Weapons_1_XLoc", "ScoreBar_P2Weapons_1_YLoc",
        "ScoreBar_P2Weapons_2_XLoc", "ScoreBar_P2Weapons_2_YLoc",
        "ScoreBar_P2Weapons_3_XLoc", "ScoreBar_P2Weapons_3_YLoc",
        "ScoreBar_Weapons_BlendAmount_NonSelected",
        "ScoreBar_Weapons_BlendAmount_Selected",
        "ScoreBar_Weapons_NonSelectedScaleFactor",
        "ScoreBar_Lives_MaxNumDisplayed"
    };
    const float values[] = {
        13, 534,41, 534,276, 495,124, 495,359, 2,3,
        495,159, 495,394, 2,4,
        467,199, 502,199, 530,199,
        467,434, 502,434, 530,434,
        16,6,0.7f,9
    };
    static_assert(sizeof(labels)/sizeof(labels[0]) == 33);
    for (int i=0;i<33;++i) game[111+i] = {labels[i], values[i]};

    const char* rlabels[] = {
        "Scorebar Player 1 Score", "Scorebar Player 1 Life Symbol",
        "Scorebar Player 1 Life Count", "Scorebar Player 1 Weapon 1",
        "Scorebar Player 1 Weapon 2", "Scorebar Player 1 Weapon 3",
        "Scorebar Player 1 Shields", "Scorebar Player 1 Power",
        "Scorebar Player 2 Score", "Scorebar Player 2 Life Symbol",
        "Scorebar Player 2 Life Count", "Scorebar Player 2 Weapon 1",
        "Scorebar Player 2 Weapon 2", "Scorebar Player 2 Weapon 3",
        "Scorebar Player 2 Shields", "Scorebar Player 2 Power"
    };
    const deimos::RectI rects[] = {
        {25,81,135,95},{98,22,138,62},{56,19,102,63},{33,181,65,216},
        {76,188,95,210},{103,188,124,210},{31,117,127,132},{31,152,127,167},
        {25,317,135,333},{98,257,138,297},{56,263,102,307},{33,416,65,451},
        {76,423,95,445},{103,423,124,445},{31,353,127,368},{31,388,127,403}
    };
    deimos::NamedTable<deimos::RectI> regions;
    for (int i=0;i<16;++i) regions.push_back({rlabels[i], rects[i]});

    std::string error;
    auto cfg = deimos::compile_legacy_score_bar_config(game, regions, &error);
    assert(cfg);
    assert(cfg->score_spacing == 13);
    assert(cfg->lives_symbol_x[0] == 534 && cfg->lives_symbol_y[1] == 276);
    assert(cfg->shield_meter_x[0] == 495 && cfg->shield_meter_y[1] == 359);
    assert(nearly(cfg->shield_increase_rate,2) && nearly(cfg->shield_decrease_rate,3));
    assert(nearly(cfg->power_increase_rate,2) && nearly(cfg->power_decrease_rate,4));
    assert(cfg->weapon_x[1][2] == 530 && cfg->weapon_y[1][2] == 434);
    assert(cfg->weapon_blend_nonselected == 16 && cfg->weapon_blend_selected == 6);
    assert(nearly(cfg->weapon_nonselected_scale,0.7f));
    assert(cfg->lives_max_displayed == 9);
    assert(cfg->panel_rects[1][7] == deimos::RectI({31,388,127,403}));
    game[143].first = "wrong";
    assert(!deimos::compile_legacy_score_bar_config(game, regions, &error));
    game[143] = {labels[32], values[32]};

    const std::string weapon_text = R"(#name_STR <Test Weapon>
#scoreBarPreviewFace_ID <wepv>
#scoreBarPreviewFrame_INT <7>
#spawn_NumUnitsToSpawn_INT <0>
)";
    auto weapon_doc = deimos::parse_tagged_text(weapon_text, &error);
    assert(weapon_doc);
    auto weapon = deimos::parse_weapon_definition_document(*weapon_doc, &error);
    assert(weapon);
    const auto preview = deimos::compile_legacy_score_bar_weapon_preview(*weapon);
    assert(preview.face == id("wepv") && preview.frame == 7);

    deimos::PlayerRuntimeSlot p;
    p.enabled = true;
    p.status = static_cast<int>(deimos::LegacyPlayerStatus::active);
    p.score = 1234;
    p.lives = 3;
    p.shield_percentage = 100;
    deimos::LegacyScoreBarWeaponInput wi;
    wi.power_percentage = 80;
    wi.previews = {{{id("gun1"),1},{id("gun2"),2},{id("gun3"),3}}};

    deimos::CompiledPlayerRuntimeDefinition player_def;
    player_def.score_bar_face=id("play"); player_def.score_bar_frame=0;
    player_def.score_bar_power_face=id("shme"); player_def.score_bar_power_frame=1;
    player_def.score_bar_shield_face=id("shme"); player_def.score_bar_shield_frame=0;
    auto state = deimos::initialize_legacy_score_bar_player(p, player_def, wi);
    assert(state.resources.base_face == id("play") && state.resources.base_frame == 0);
    assert(state.resources.power_face == id("shme") && state.resources.power_frame == 1);
    assert(state.resources.shield_face == id("shme") && state.resources.shield_frame == 0);
    assert(state.cached_score == 1234 && state.cached_lives == 3);
    assert(nearly(state.displayed_shield,0) && nearly(state.displayed_power,0));
    assert(state.dirty.score && state.dirty.life_symbol && state.dirty.life_count &&
           state.dirty.weapons && state.dirty.shield && state.dirty.power);
    assert(state.present_latch && state.content_visible);

    // First active tick converges upward at exact 2/2 rates.
    deimos::advance_legacy_score_bar_player(state,p,wi,*cfg);
    assert(nearly(state.displayed_shield,2));
    assert(nearly(state.displayed_power,2));
    assert(state.dirty.shield && state.dirty.power);
    assert(!state.dirty.score && !state.dirty.life_count && !state.dirty.weapons);

    // Score/lives and preview dirty classes are independent; life symbol is not
    // redrawn merely because the semantic lives count changes.
    p.score = 2000; p.lives = 4;
    wi.previews_changed = true;
    wi.previews[0] = {id("new1"),7};
    deimos::advance_legacy_score_bar_player(state,p,wi,*cfg);
    assert(state.dirty.score && state.dirty.life_count && state.dirty.weapons);
    assert(!state.dirty.life_symbol);
    assert(state.weapon_previews[0].face == id("new1"));

    // Waiting/dying states permit decreases but suppress increases.
    p.status = static_cast<int>(deimos::LegacyPlayerStatus::waiting);
    state.displayed_shield = 50; p.shield_percentage = 20;
    state.displayed_power = 50; wi.power_percentage = 20; wi.previews_changed=false;
    deimos::advance_legacy_score_bar_player(state,p,wi,*cfg);
    assert(nearly(state.displayed_shield,47));
    assert(nearly(state.displayed_power,46));
    state.displayed_shield=20; p.shield_percentage=80;
    state.displayed_power=20; wi.power_percentage=80;
    deimos::advance_legacy_score_bar_player(state,p,wi,*cfg);
    assert(nearly(state.displayed_shield,20));
    assert(nearly(state.displayed_power,20));

    // The PEF uses literal 0 and 100 bounds on displayed power.
    p.status = static_cast<int>(deimos::LegacyPlayerStatus::active);
    state.displayed_power = 99; wi.power_percentage=200;
    deimos::advance_legacy_score_bar_player(state,p,wi,*cfg);
    assert(nearly(state.displayed_power,100));
    state.displayed_power = 1; wi.power_percentage=-200;
    deimos::advance_legacy_score_bar_player(state,p,wi,*cfg);
    assert(nearly(state.displayed_power,0));

    assert(deimos::legacy_score_bar_displayed_lives(10,*cfg) == 9);
    assert(deimos::legacy_score_bar_displayed_lives(3,*cfg) == 2);
    assert(deimos::legacy_score_bar_displayed_lives(1,*cfg) == 0);
    assert(deimos::legacy_score_bar_displayed_lives(0,*cfg) == 0);

    // First transition to absent/game-over dirties all six regions exactly once.
    p.status = static_cast<int>(deimos::LegacyPlayerStatus::game_over);
    deimos::advance_legacy_score_bar_player(state,p,wi,*cfg);
    assert(state.dirty.score && state.dirty.life_symbol && state.dirty.life_count &&
           state.dirty.weapons && state.dirty.shield && state.dirty.power);
    assert(!state.present_latch && !state.content_visible);
    deimos::advance_legacy_score_bar_player(state,p,wi,*cfg);
    assert(!state.dirty.any());

    std::cout << "score-bar runtime tests passed\n";
}
