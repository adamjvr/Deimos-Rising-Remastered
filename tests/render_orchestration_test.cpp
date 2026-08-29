#include "deimos/render_orchestration.hpp"
#include "deimos/terrain_runtime.hpp"

#include <cassert>
#include <iostream>
#include <memory>

using namespace deimos;
namespace {
constexpr FourCC id(char a,char b,char c,char d){ return FourCC{{a,b,c,d}}; }

LegacySpriteGroupMetadata make_group() {
    LegacySpriteGroupMetadata g;
    g.id=id('T','E','S','T');
    LegacySpriteFrameMetadata f;
    f.width=4; f.height=2; f.transparent_key=0x001f;
    f.color_pixels.assign(8,0x7c00);
    f.transparency.assign(8,0);
    g.frames.push_back(f);
    return g;
}
}

int main(){
    // Exact 0x100B0 horizontal view stepping and saturation.
    LegacyHorizontalViewRuntime view;
    step_legacy_horizontal_view(view,true);
    assert(view.offset==1 && view.direction==1);
    for(int i=0;i<30;++i) step_legacy_horizontal_view(view,true);
    assert(view.offset==31 && view.direction==1);
    step_legacy_horizontal_view(view,true);
    assert(view.offset==31 && view.direction==0);
    for(int i=0;i<63;++i) step_legacy_horizontal_view(view,false);
    assert(view.offset==-32 && view.direction==-1);
    step_legacy_horizontal_view(view,false);
    assert(view.offset==-32 && view.direction==0);

    LegacySpriteCache cache;
    auto group=make_group();
    assert(cache.publish(std::move(group)));

    LegacySpriteVisualRuntime r;
    r.sprite_face=id('T','E','S','T'); r.sprite_frame=0; r.draw_layer=id('a','i','l','o');
    r.air_domain=true; r.world_space=true; r.casts_shadows=true;
    r.visibility_percent=80.0f; r.scale=1.5f;
    r.tint_percent=25.0f; r.tint_color={255,128,0};
    r.collision_glow_active=true; r.collision_glow_amount_0_to_32=7;
    r.collision_glow_color={0,255,0};

    LegacyRenderOrchestrationContext c;
    c.world_x=100.75f; c.world_y=50.9f; c.horizontal_view_offset=7;
    c.world_y_origin=400; c.render_sequence=9; c.clip={0,0,300,300};
    LegacyShadowRuntimeConfig sc;
    auto batch=build_legacy_raster_requests(r,cache,sc,c);
    assert(batch.requests.size()==4); // shadow, base, tint, glow
    const auto& sh=batch.requests[0];
    assert((sh.flags & kLegacyRenderShadow)!=0);
    assert(sh.numeric_layer==6);
    assert(sh.effect_amount_0_to_32==20);
    assert(sh.scale==0.75f);
    // Air offsets use fixed 0.5 basis when adjustShadowLocForScaling is false:
    // x=trunc(100.75-24-7)=69, y=trunc(50.9+52)=102.
    assert(sh.center_x==69 && sh.center_y==102);

    const auto& base=batch.requests[1];
    assert(base.center_x==93 && base.center_y==50); // trunc(100.75)-7, trunc(50.9)
    assert(base.numeric_layer==7);
    assert((base.flags & kLegacyRenderOverallTransparency)!=0);
    assert(base.effect_amount_0_to_32==6);
    assert(base.scale==1.5f);
    assert(!base.immediate);

    const auto& tint=batch.requests[2];
    assert(tint.flags==kLegacyRenderSolidColor);
    assert(tint.effect_amount_0_to_32==25);
    assert(tint.effect_color==legacy_rgb24_to_rgb555({255,128,0}));
    assert(tint.effect_color==static_cast<std::uint16_t>((31<<10)|(16<<5)));
    const auto& glow=batch.requests[3];
    assert(glow.flags==kLegacyRenderSolidColor && glow.effect_amount_0_to_32==7);

    // HUD/non-world sprites bypass 0x100A0.
    r.world_space=false; r.casts_shadows=false; r.tint_percent=0; r.collision_glow_active=false;
    r.visibility_percent=100.0f; r.draw_layer=id('h','u','d',' ');
    batch=build_legacy_raster_requests(r,cache,sc,c);
    assert(batch.requests.size()==1);
    assert(batch.requests[0].center_x==100 && batch.requests[0].numeric_layer==15);
    assert(batch.requests[0].flags==0);

    // Terrain main path uses +32 X / world-Y-origin and the one-per-sequence
    // +0x90 gate. Shadow remains its own layer-0 terrain request.
    r.world_space=true; r.casts_shadows=true; r.draw_to_terrain=true;
    r.draw_layer=id('g','r','o','u'); r.air_domain=false; r.visibility_percent=100;
    r.scale=1.0f; r.last_terrain_submit_sequence=0;
    c.render_sequence=10;
    batch=build_legacy_raster_requests(r,cache,sc,c);
    assert(batch.terrain_main_marked_this_sequence);
    assert(batch.requests.size()==2);
    assert(batch.requests[0].numeric_layer==0);
    assert((batch.requests[0].flags & (kLegacyRenderShadow|kLegacyRenderTerrainTarget)) ==
           (kLegacyRenderShadow|kLegacyRenderTerrainTarget));
    const auto& terrain= batch.requests[1];
    assert(terrain.numeric_layer==1);
    assert(terrain.flags==kLegacyRenderTerrainTarget);
    assert(terrain.center_x==132 && terrain.center_y==450);
    assert(r.last_terrain_submit_sequence==10);
    // Same sequence: no repeated persistent main write; shadow request still exists.
    batch=build_legacy_raster_requests(r,cache,sc,c);
    assert(!batch.terrain_main_marked_this_sequence);
    assert(batch.requests.size()==1 && batch.requests[0].numeric_layer==0);

    // End-to-end semantic request -> queue -> recovered compositor.
    r.draw_to_terrain=false; r.casts_shadows=false; r.visibility_percent=100;
    r.draw_layer=id('g','r','o','u'); r.world_space=false; r.scale=1.0f;
    c.world_x=5; c.world_y=5; c.clip={0,0,12,12}; c.immediate=false;
    LegacyRenderQueue queue; LegacyRasterSurface main(12,12,0), terr(12,12,0);
    auto results=submit_legacy_sprite_render(r,cache,sc,c,queue,main,terr);
    assert(results.size()==1 && results[0]==LegacyRasterResult::skipped);
    assert(queue.size(3)==1);
    auto flushed=queue.flush_group(1,main,terr);
    assert(flushed.size()==1 && flushed[0]==LegacyRasterResult::drawn);
    assert(main.pixels[4*12+3]==0x7c00);

    std::cout << "render_orchestration_test: PASS\n";
}
