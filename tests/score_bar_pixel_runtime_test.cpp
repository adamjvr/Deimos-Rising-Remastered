#include "deimos/image16_resource.hpp"
#include "deimos/score_bar_runtime.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {
deimos::FourCC id(const char* s) { return deimos::FourCC{{s[0],s[1],s[2],s[3]}}; }

deimos::LegacySpriteFrameMetadata frame(int w, int h, std::uint16_t color = 0x7fff) {
    deimos::LegacySpriteFrameMetadata f;
    f.width = w; f.height = h; f.source_rect = {0,0,w,h};
    f.transparent_key = 0;
    f.color_pixels.assign(static_cast<std::size_t>(w*h), color);
    f.transparency.assign(static_cast<std::size_t>(w*h), 0);
    return f;
}

deimos::LegacySpriteGroupMetadata font_group() {
    static constexpr std::array<int,91> widths = {{
        7,7,7,7,6,6,7,7,4,5,7,5,10,7,7,7,7,7,7,6,7,7,11,6,7,6,
        7,7,6,7,7,4,7,7,4,4,6,4,10,7,7,7,7,5,6,4,7,7,10,7,7,6,
        5,6,7,7,7,7,6,7,7,7,4,5,8,7,9,8,3,5,5,6,7,3,4,4,6,4,4,
        7,7,7,7,10,6,6,7,4,4,6,4
    }};
    deimos::LegacySpriteGroupMetadata g; g.id=id("tesm");
    for (int w : widths) g.frames.push_back(frame(w,13));
    return g;
}

deimos::LegacyScoreBarConfig config() {
    deimos::LegacyScoreBarConfig c;
    c.panel_rects[0] = {{{25,81,135,95},{98,22,138,62},{56,19,102,63},{33,181,65,216},
                         {76,188,95,210},{103,188,124,210},{31,117,127,132},{31,152,127,167}}};
    c.panel_rects[1] = {{{25,317,135,333},{98,257,138,297},{56,263,102,307},{33,416,65,451},
                         {76,423,95,445},{103,423,124,445},{31,353,127,368},{31,388,127,403}}};
    return c;
}

deimos::TextFormatDefinition text_style(int x,int y,deimos::Rgb24 color,int spacing=0) {
    deimos::TextFormatDefinition t;
    t.x=x; t.y=y; t.format_token="CENT"; t.monospaced=true;
    t.blend_amount_0_to_32=0; t.space_between_chars=spacing;
    t.colorise=true; t.colorise_color=color;
    return t;
}
}

int main() {
    // Uncompressed bottom-origin 16-bit TGA normalizes to top-left origin.
    std::vector<std::uint8_t> tga(18 + 8, 0);
    tga[2]=2; tga[12]=2; tga[14]=2; tga[16]=16; tga[17]=1;
    const std::uint16_t words[4] = {1,2,3,4}; // file bottom row then top row
    for (int i=0;i<4;++i) { tga[18+i*2]=static_cast<std::uint8_t>(words[i]); tga[19+i*2]=0; }
    std::string error;
    auto image=deimos::decode_legacy_tga16(tga,&error);
    assert(image && image->width==2 && image->height==2);
    assert((image->pixels == std::vector<std::uint16_t>{3,4,1,2}));
    tga[2]=10; assert(!deimos::decode_legacy_tga16(tga,&error)); tga[2]=2;

    const auto font=font_group();
    assert(deimos::legacy_small_text_frame_for_char('A') == 0);
    assert(deimos::legacy_small_text_frame_for_char('z') == 51);
    assert(deimos::legacy_small_text_frame_for_char('1') == 52);
    assert(deimos::legacy_small_text_frame_for_char('9') == 60);
    assert(deimos::legacy_small_text_frame_for_char('0') == 61);
    assert(deimos::legacy_small_text_frame_for_char('[') == 69);
    assert(deimos::legacy_small_text_frame_for_char(']') == 70);
    assert(deimos::legacy_small_text_frame_for_char('~') == 89);
    assert(!deimos::legacy_small_text_frame_for_char(' '));
    assert(deimos::legacy_rgb24_to_rgb555({0x94,0xde,0xe6}) == 0x4b7c);
    assert(deimos::legacy_rgb24_to_rgb555({0xff,0,0}) == 0x7c00);

    auto score_style=text_style(494,83,{0x94,0xde,0xe6},4);
    auto score_reqs=deimos::build_legacy_small_text_requests(font,score_style,"0012345");
    assert(score_reqs.size()==7);
    assert(score_reqs[0].sprite_frame==61 && score_reqs[0].center_x==462 && score_reqs[0].center_y==89);
    assert(score_reqs[1].center_x==473);
    assert(score_reqs[2].sprite_frame==52 && score_reqs[2].center_x==483);
    assert(score_reqs.back().flags == deimos::kLegacyRenderSolidColor);
    assert(score_reqs.back().effect_color == 0x4b7c && score_reqs.back().effect_amount_0_to_32==0);
    auto faded=deimos::build_legacy_small_text_requests(font,score_style,"0",true);
    assert(faded.size()==1 && faded[0].effect_amount_0_to_32==16);
    assert(deimos::format_legacy_score_value(12345)=="0012345");
    assert(deimos::format_legacy_lives_value(2)=="2");

    auto cfg=config();
    deimos::LegacyScoreBarTextStyles styles;
    styles.score[0]=score_style;
    styles.score[1]=text_style(494,318,{0x94,0xde,0xe6},4);
    styles.lives[0]=text_style(499,50,{0x94,0xde,0xe6});
    styles.lives[1]=text_style(498,285,{0x94,0xde,0xe6});
    styles.last_life[0]=text_style(499,50,{0xff,0,0});
    styles.last_life[1]=text_style(498,285,{0xff,0,0});
    styles.shield.color_strip=true; styles.shield.color_strip_blend_amount_0_to_32=8;
    styles.power=styles.shield;

    deimos::LegacySpriteCache cache;
    deimos::LegacySpriteGroupMetadata play; play.id=id("play"); play.frames.push_back(frame(40,40,0x03e0));
    deimos::LegacySpriteGroupMetadata shme; shme.id=id("shme");
    shme.frames.push_back(frame(96,15,0x7fff)); shme.frames.push_back(frame(96,15,0x7c00));
    deimos::LegacySpriteGroupMetadata g1; g1.id=id("gun1"); g1.frames.push_back(frame(32,35,0x001f));
    deimos::LegacySpriteGroupMetadata g2; g2.id=id("gun2"); g2.frames.push_back(frame(19,22,0x03e0));
    deimos::LegacySpriteGroupMetadata g3; g3.id=id("gun3"); g3.frames.push_back(frame(21,22,0x7c00));
    assert(cache.publish(std::move(play)) && cache.publish(std::move(shme)) && cache.publish(std::move(g1)) &&
           cache.publish(std::move(g2)) && cache.publish(std::move(g3)));

    deimos::LegacyRasterSurface base(160,480,0x1234);
    deimos::LegacyRasterSurface canvas(576,480,0x7777);
    deimos::LegacyScoreBarPlayerState state;
    state.resources={id("play"),0,id("shme"),1,id("shme"),0};
    state.cached_score=12345; state.cached_lives=1; state.displayed_shield=50; state.displayed_power=100;
    state.weapon_previews={{{id("gun1"),0},{id("gun2"),0},{id("gun3"),0}}};
    state.dirty={true,true,true,true,true,true}; state.content_visible=true;
    deimos::LegacyScoreBarRenderAssets assets{&base,&font,&cache};
    assert(deimos::rasterize_legacy_score_bar_player(0,state,cfg,styles,assets,canvas,&error));
    // Last life is red, and regions outside the 160px score bar are untouched.
    assert(canvas.pixels[0] == 0x7777);
    bool saw_red=false;
    for (int y=50;y<63;++y) for (int x=472;x<518;++x)
        saw_red |= canvas.pixels[static_cast<std::size_t>(y)*canvas.width+x] == 0x7c00;
    assert(saw_red);
    // A 50% shield COST pass darkens the left half of its restored/drawn region;
    // 100% power deliberately skips the COST overlay, matching 0x323CC/0x3267C.
    const auto shield_left=canvas.pixels[124u*canvas.width+447u];
    const auto shield_right=canvas.pixels[124u*canvas.width+542u];
    assert(shield_left != shield_right);
    assert(canvas.pixels[159u*canvas.width+495u] == 0x7c00);

    // Locked/unavailable weapon slots in the original HUD are represented by
    // an absent descriptor after restoring the static score-bar panel. They
    // must not be treated as missing-sprite failures.
    deimos::LegacyRasterSurface locked_canvas(576,480,0x7777);
    auto locked_state = state;
    locked_state.weapon_previews = {{{id("gun1"),0},{id("none"),0},{deimos::FourCC{},0}}};
    locked_state.dirty = {false,false,false,true,false,false};
    assert(deimos::rasterize_legacy_score_bar_player(
        0,locked_state,cfg,styles,assets,locked_canvas,&error));

    std::cout << "score-bar pixel runtime tests passed\n";
}
