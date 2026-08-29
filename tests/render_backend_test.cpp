#include "deimos/render_backend.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>

using namespace deimos;

namespace {
constexpr FourCC id(char a,char b,char c,char d){ return FourCC{{a,b,c,d}}; }

LegacySpriteFrameMetadata frame3() {
    LegacySpriteFrameMetadata f;
    f.width=3; f.height=3; f.transparent_key=0x001f;
    f.color_pixels={
        0x7c00,0x03e0,0x001f,
        0x7fff,0x4210,0x2108,
        0x001f,0x7c00,0x03e0,
    };
    // first row fully opaque; middle includes half transparency and transparent;
    // final row uses the original whole-row fast-skip sentinel.
    f.transparency={
        0,0,0,
        0,16,32,
        1000,32,32,
    };
    return f;
}
}

int main() {
    assert(legacy_blend_rgb555(0x0000,0x7fff,0) == 0x7fff);
    assert(legacy_blend_rgb555(0x0000,0x7fff,32) == 0x0000);
    assert(legacy_blend_rgb555(0x0000,0x7fff,16) == 0x3def);
    assert(legacy_scale_rgb555(0x7fff,16) == 0x3def);

    auto f=frame3();
    LegacyRasterRequest q;
    q.frame=&f; q.sprite_face=id('T','E','S','T'); q.center_x=2; q.center_y=2;
    q.clip={0,0,5,5}; q.scale=1.0f;

    // Normal 0x1D9F0-style source composition, including mask blend and row skip.
    LegacyRasterSurface s(5,5,0);
    assert(rasterize_legacy_request(q,s)==LegacyRasterResult::drawn);
    assert(s.pixels[1*5+1]==0x7c00);
    assert(s.pixels[1*5+2]==0x03e0);
    assert(s.pixels[2*5+1]==0x7fff);
    assert(s.pixels[2*5+2]==legacy_blend_rgb555(0,0x4210,16));
    assert(s.pixels[2*5+3]==0); // mask 32
    assert(s.pixels[3*5+1]==0); // row sentinel 1000

    // Overall transparency adds to per-pixel transparency rather than multiplying.
    s=LegacyRasterSurface(5,5,0x7c00);
    q.flags=kLegacyRenderOverallTransparency; q.effect_amount_0_to_32=8;
    assert(rasterize_legacy_request(q,s)==LegacyRasterResult::drawn);
    assert(s.pixels[1*5+2]==legacy_blend_rgb555(0x7c00,0x03e0,8));
    assert(s.pixels[2*5+2]==legacy_blend_rgb555(0x7c00,0x4210,24)); // 8+16

    // Shadow mode darkens destination. For partial coverage the original uses
    // trunc(base + 0.032f * mask^2), so base 20 + mask16 => trunc(28.192)=28.
    s=LegacyRasterSurface(5,5,0x7fff);
    q.flags=kLegacyRenderShadow; q.effect_amount_0_to_32=20;
    assert(rasterize_legacy_request(q,s)==LegacyRasterResult::drawn);
    assert(s.pixels[1*5+1]==legacy_scale_rgb555(0x7fff,20));
    assert(s.pixels[2*5+2]==legacy_scale_rgb555(0x7fff,28));
    assert(s.pixels[2*5+3]==0x7fff);

    // Tint/glow is a solid-color overlay using the same additive transparency.
    s=LegacyRasterSurface(5,5,0);
    q.flags=kLegacyRenderSolidColor; q.effect_amount_0_to_32=4; q.effect_color=0x7c00;
    assert(rasterize_legacy_request(q,s)==LegacyRasterResult::drawn);
    assert(s.pixels[1*5+1]==legacy_blend_rgb555(0,0x7c00,4));
    assert(s.pixels[2*5+2]==legacy_blend_rgb555(0,0x7c00,20));

    // QuickDraw right/bottom are exclusive: narrow clip only touches center pixel.
    s=LegacyRasterSurface(5,5,0);
    q.flags=0; q.effect_amount_0_to_32=0; q.clip={2,2,3,3};
    assert(rasterize_legacy_request(q,s)==LegacyRasterResult::drawn);
    assert(s.pixels[2*5+2]==legacy_blend_rgb555(0,0x4210,16));
    assert(s.pixels[1*5+1]==0);

    // Global "Sprite Alpha Drawing" toggle forces the color-key fallback even
    // when the reconstructed frame carries a secondary transparency plane.
    s=LegacyRasterSurface(5,5,0);
    q.flags=0; q.effect_amount_0_to_32=0; q.clip={0,0,5,5}; q.scale=1.0f;
    LegacyRasterConfig no_alpha; no_alpha.alpha_drawing_enabled=false;
    assert(rasterize_legacy_request(q,s,no_alpha)==LegacyRasterResult::drawn);
    // Frame (0,2) is color-key blue but mask 0 when alpha drawing is enabled;
    // disabled alpha drawing therefore skips it instead of copying it.
    assert(s.pixels[1*5+3]==0);

    // Transparent-key fallback when no secondary plane exists.
    auto key=f; key.transparency.clear();
    s=LegacyRasterSurface(5,5,0);
    q.frame=&key; q.clip={0,0,5,5};
    assert(rasterize_legacy_request(q,s)==LegacyRasterResult::drawn);
    assert(s.pixels[1*5+3]==0); // 0x001f == transparent key
    assert(s.pixels[1*5+1]==0x7c00);

    // 0x1A6F0 nearest-neighbour scaling uses integer-ratio source mapping.
    // A 3x3 frame at 2x becomes 6x6; each source pixel occupies 2x2.
    q.scale=2.0f; q.center_x=3; q.center_y=3; q.clip={0,0,6,6}; q.flags=0;
    s=LegacyRasterSurface(6,6,0);
    assert(rasterize_legacy_request(q,s)==LegacyRasterResult::drawn);
    assert(s.pixels[0]==0x7c00 && s.pixels[1]==0x7c00);
    assert(s.pixels[2]==0x03e0 && s.pixels[3]==0x03e0);
    assert(s.pixels[2*6+0]==0x7fff); // source row 1 begins here
    assert(s.pixels[4*6+0]==0);      // source row 2 has 1000 row sentinel

    // Fractional centering uses raw scaled dimensions before fctiwz. 3*1.5=4.5,
    // center 4 -> left=trunc(1.75)=1, width=trunc(4.5)=4.
    q.scale=1.5f; q.center_x=4; q.center_y=4; q.clip={0,0,8,8};
    s=LegacyRasterSurface(8,8,0);
    assert(rasterize_legacy_request(q,s)==LegacyRasterResult::drawn);
    assert(s.pixels[1*8+1]==0x7c00);
    assert(s.pixels[1*8+2]==0x7c00); // sx=floor(3*1/4)=0
    assert(s.pixels[1*8+3]==0x03e0); // sx=floor(3*2/4)=1
    q.scale=1.0f; q.center_x=2; q.center_y=2; q.clip={0,0,5,5};

    // Dormant COST path: clipped solid rectangle.
    LegacyRasterRequest cost;
    cost.sprite_face=id('C','O','S','T'); cost.special_rect={1,1,4,4};
    cost.clip={2,2,5,5}; cost.special_color=0x03e0; cost.effect_amount_0_to_32=0;
    s=LegacyRasterSurface(5,5,0);
    assert(rasterize_legacy_request(cost,s)==LegacyRasterResult::drawn);
    assert(s.pixels[2*5+2]==0x03e0 && s.pixels[3*5+3]==0x03e0);
    assert(s.pixels[1*5+1]==0);

    // 0x1A450/0x1A650 queue semantics and terrain destination flag.
    LegacyRenderQueue queue;
    q.frame=&f; q.sprite_face=id('T','E','S','T'); q.clip={0,0,5,5};
    q.numeric_layer=1; q.flags=kLegacyRenderTerrainTarget; q.center_x=2; q.center_y=2;
    queue.enqueue(q);
    assert(queue.size(1)==1);
    LegacyRasterSurface main(5,5,0), terrain(5,5,0);
    auto r=queue.flush_group(0,main,terrain);
    assert(r.size()==1 && r[0]==LegacyRasterResult::drawn);
    assert(main.pixels[1*5+1]==0 && terrain.pixels[1*5+1]==0x7c00);
    // Layers 0/1 are one-shot: second flush finds the face replaced by none.
    assert(queue.flush_group(0,main,terrain).empty());

    // 0x18A40/0x19570 direct-vs-queue split.
    LegacyRasterRequest submitted=q;
    submitted.numeric_layer=3; submitted.flags=0; submitted.immediate=false;
    const auto before=main.pixels[1*5+1];
    assert(submit_legacy_render_request(submitted,queue,main,terrain)==LegacyRasterResult::skipped);
    assert(queue.size(3)==1 && main.pixels[1*5+1]==before);
    assert(queue.flush_group(1,main,terrain).size()==1);
    submitted.immediate=true; submitted.center_x=2; submitted.center_y=2; submitted.clip={0,0,5,5};
    assert(submit_legacy_render_request(submitted,queue,main,terrain)==LegacyRasterResult::drawn);
    // "Sprite FX Disabled" wrapper copy forces scale=1 and effect amount=0.
    LegacyRasterConfig no_fx; no_fx.sprite_fx_enabled=false;
    submitted.flags=kLegacyRenderOverallTransparency; submitted.scale=2.0f;
    submitted.effect_amount_0_to_32=31;
    main=LegacyRasterSurface(5,5,0);
    assert(submit_legacy_render_request(submitted,queue,main,terrain,no_fx)==LegacyRasterResult::drawn);
    // With FX disabled this is an opaque unscaled copy, not an almost-transparent 2x draw.
    assert(main.pixels[1*5+1]==0x7c00);

    q.numeric_layer=7; q.flags=0;
    queue.enqueue(q);
    assert(queue.flush_group(2,main,terrain).size()==1);
    // Non-one-shot layers remain resident and draw again on another flush.
    assert(queue.flush_group(2,main,terrain).size()==1);

    std::cout << "render_backend_test: PASS\n";
    return 0;
}
