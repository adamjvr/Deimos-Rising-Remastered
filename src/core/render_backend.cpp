#include "deimos/render_backend.hpp"

#include <algorithm>
#include <cmath>

namespace deimos {
namespace {
constexpr FourCC fourcc(char a, char b, char c, char d) { return FourCC{{a,b,c,d}}; }

int clamp32(int v) { return std::clamp(v, 0, 32); }

std::uint16_t blend_channels(std::uint16_t dst, std::uint16_t src, int t) {
    t = clamp32(t);
    const int s = 32 - t;
    const int dr = (dst >> 10) & 31, dg = (dst >> 5) & 31, db = dst & 31;
    const int sr = (src >> 10) & 31, sg = (src >> 5) & 31, sb = src & 31;
    const int r = (dr * t + sr * s) >> 5;
    const int g = (dg * t + sg * s) >> 5;
    const int b = (db * t + sb * s) >> 5;
    return static_cast<std::uint16_t>((r << 10) | (g << 5) | b);
}

std::uint16_t scale_channels(std::uint16_t p, int factor) {
    factor = clamp32(factor);
    const int r = (((p >> 10) & 31) * factor) >> 5;
    const int g = (((p >> 5) & 31) * factor) >> 5;
    const int b = ((p & 31) * factor) >> 5;
    return static_cast<std::uint16_t>((r << 10) | (g << 5) | b);
}

LegacyRasterRect intersect_rect(LegacyRasterRect a, LegacyRasterRect b) {
    return {std::max(a.top,b.top), std::max(a.left,b.left),
            std::min(a.bottom,b.bottom), std::min(a.right,b.right)};
}

bool absent(FourCC id) { return id == fourcc('n','o','n','e'); }

int shadow_factor(int base, int mask) {
    if (mask <= 0) return clamp32(base);
    // 0x1DDF0 loads literal 0.032f and executes single-precision
    // factor = trunc(base + mask * (0.032f * mask)).
    const float m = static_cast<float>(mask);
    const float inner = 0.032f * m;
    const float combined = m * inner + static_cast<float>(base);
    return static_cast<int>(std::trunc(combined));
}

LegacyRasterResult raster_sprite(const LegacyRasterRequest& q, LegacyRasterSurface& dst, LegacyRasterConfig config) {
    if (!dst.valid()) return LegacyRasterResult::invalid_surface;
    if (absent(q.sprite_face) || q.effect_amount_0_to_32 == 32) return LegacyRasterResult::skipped;
    if (!q.frame || !q.frame->has_surface()) return LegacyRasterResult::missing_frame;

    const auto& f = *q.frame;
    int draw_w = f.width;
    int draw_h = f.height;
    int left = q.center_x - (f.width / 2);
    int top = q.center_y - (f.height / 2);
    if (q.scale != 1.0f) {
        const float raw_w = static_cast<float>(f.width) * q.scale;
        const float raw_h = static_cast<float>(f.height) * q.scale;
        draw_w = static_cast<int>(std::trunc(raw_w));
        draw_h = static_cast<int>(std::trunc(raw_h));
        // 0x1A6F0 uses the untruncated scaled float for centering, then fctiwz,
        // while the right/bottom edge is left/top + truncated scaled extent.
        left = static_cast<int>(std::trunc(static_cast<float>(q.center_x) - raw_w * 0.5f));
        top = static_cast<int>(std::trunc(static_cast<float>(q.center_y) - raw_h * 0.5f));
    }
    if (draw_w <= 0 || draw_h <= 0) return LegacyRasterResult::skipped;

    const LegacyRasterRect sprite{top, left, top + draw_h, left + draw_w};
    const auto clipped = intersect_rect(intersect_rect(sprite, q.clip), dst.bounds());
    if (clipped.empty()) return LegacyRasterResult::skipped;

    int mode = 0;
    if (q.flags & kLegacyRenderOverallTransparency) mode = 1;
    else if (q.flags & kLegacyRenderShadow) mode = 2;
    else if (q.flags & kLegacyRenderSolidColor) mode = 3;

    const bool plane = config.alpha_drawing_enabled && f.has_transparency_plane() &&
        f.transparency.size() == f.color_pixels.size();
    bool wrote = false;
    for (int y = clipped.top; y < clipped.bottom; ++y) {
        // 0x1B7D0 family: sourceY = srcH * (destY-top) / scaledH.
        const int sy = (f.height * (y - top)) / draw_h;
        const std::size_t row = static_cast<std::size_t>(sy) * static_cast<std::size_t>(f.width);
        if (plane && f.transparency[row] == 1000) continue;
        for (int x = clipped.left; x < clipped.right; ++x) {
            // Horizontal source byte offsets are precomputed by the original
            // as 2 * (srcW * (destX-left) / scaledW).
            const int sx = (f.width * (x - left)) / draw_w;
            const std::size_t si = row + static_cast<std::size_t>(sx);
            const std::size_t di = static_cast<std::size_t>(y) * static_cast<std::size_t>(dst.width) + static_cast<std::size_t>(x);
            const std::uint16_t src = f.color_pixels[si];
            int mask = 0;
            if (plane) {
                mask = f.transparency[si];
                if (mask == 1000 || mask >= 32) continue;
            } else if (src == f.transparent_key) {
                continue;
            }

            switch (mode) {
            case 0:
                dst.pixels[di] = (mask == 0) ? src : blend_channels(dst.pixels[di], src, mask);
                wrote = true;
                break;
            case 1: {
                const int effective = q.effect_amount_0_to_32 + mask;
                if (effective >= 32) break;
                dst.pixels[di] = blend_channels(dst.pixels[di], src, effective);
                wrote = true;
                break;
            }
            case 2: {
                const int factor = shadow_factor(q.effect_amount_0_to_32, mask);
                if (factor >= 32) break;
                dst.pixels[di] = scale_channels(dst.pixels[di], factor);
                wrote = true;
                break;
            }
            case 3: {
                const int effective = q.effect_amount_0_to_32 + mask;
                if (effective >= 32) break;
                dst.pixels[di] = blend_channels(dst.pixels[di], q.effect_color, effective);
                wrote = true;
                break;
            }
            }
        }
    }
    return wrote ? LegacyRasterResult::drawn : LegacyRasterResult::skipped;
}
}

std::uint16_t legacy_blend_rgb555(std::uint16_t dst, std::uint16_t src, int t) {
    return blend_channels(dst, src, t);
}

std::uint16_t legacy_scale_rgb555(std::uint16_t p, int factor) {
    return scale_channels(p, factor);
}

LegacyRasterResult rasterize_legacy_solid_rect(
    LegacyRasterSurface& dst, LegacyRasterRect rect, LegacyRasterRect clip,
    std::uint16_t color, int transparency) {
    if (!dst.valid()) return LegacyRasterResult::invalid_surface;
    if (transparency >= 32) return LegacyRasterResult::skipped;
    const auto r = intersect_rect(intersect_rect(rect, clip), dst.bounds());
    if (r.empty()) return LegacyRasterResult::skipped;
    for (int y=r.top; y<r.bottom; ++y) {
        for (int x=r.left; x<r.right; ++x) {
            auto& p = dst.pixels[static_cast<std::size_t>(y)*dst.width + x];
            p = blend_channels(p, color, transparency);
        }
    }
    return LegacyRasterResult::drawn;
}

LegacyRasterResult rasterize_legacy_request(
    const LegacyRasterRequest& q, LegacyRasterSurface& destination, LegacyRasterConfig config) {
    if (q.sprite_face == fourcc('C','O','S','T')) {
        return rasterize_legacy_solid_rect(destination, q.special_rect, q.clip,
                                           q.special_color, q.effect_amount_0_to_32);
    }
    return raster_sprite(q, destination, config);
}

void LegacyRenderQueue::enqueue(LegacyRasterRequest request) {
    if (request.numeric_layer >= layers_.size()) return;
    request.immediate = true; // queued record +0x31 is forced to 1 by 0x1A450.
    layers_[request.numeric_layer].push_back(request);
}

std::size_t LegacyRenderQueue::size(std::uint8_t layer) const {
    return layer < layers_.size() ? layers_[layer].size() : 0;
}

std::vector<LegacyRasterResult> LegacyRenderQueue::flush_layer(
    std::uint8_t layer, LegacyRasterSurface& main, LegacyRasterSurface& terrain, LegacyRasterConfig config) {
    std::vector<LegacyRasterResult> out;
    if (layer >= layers_.size()) return out;
    for (auto& q : layers_[layer]) {
        if (absent(q.sprite_face) || q.numeric_layer != layer) continue;
        auto& target = (q.flags & kLegacyRenderTerrainTarget) ? terrain : main;
        out.push_back(rasterize_legacy_request(q, target, config));
        if (layer == 0 || layer == 1) q.sprite_face = fourcc('n','o','n','e');
    }
    return out;
}

std::vector<LegacyRasterResult> LegacyRenderQueue::flush_group(
    std::uint8_t group, LegacyRasterSurface& main, LegacyRasterSurface& terrain, LegacyRasterConfig config) {
    std::vector<LegacyRasterResult> out;
    int first=0, last=-1;
    if (group == 0) { first=0; last=1; }
    else if (group == 1) { first=2; last=5; }
    else if (group == 2) { first=6; last=15; }
    for (int l=first; l<=last; ++l) {
        auto r = flush_layer(static_cast<std::uint8_t>(l), main, terrain, config);
        out.insert(out.end(), r.begin(), r.end());
    }
    return out;
}

void LegacyRenderQueue::clear() {
    for (auto& q : layers_) q.clear();
}


LegacyRasterResult submit_legacy_render_request(
    LegacyRasterRequest request,
    LegacyRenderQueue& queue,
    LegacyRasterSurface& main_surface,
    LegacyRasterSurface& terrain_surface,
    LegacyRasterConfig config) {
    if (absent(request.sprite_face) || request.effect_amount_0_to_32 == 32) {
        return LegacyRasterResult::skipped;
    }
    if (!config.sprite_fx_enabled) {
        request.scale = 1.0f;
        request.effect_amount_0_to_32 = 0;
    }
    if (!request.immediate) {
        queue.enqueue(request);
        // The legacy wrapper returns after queueing; "drawn" is reserved for
        // an actual surface mutation, so queued work reports skipped here.
        return LegacyRasterResult::skipped;
    }
    auto& target = (request.flags & kLegacyRenderTerrainTarget) ? terrain_surface : main_surface;
    return rasterize_legacy_request(request, target, config);
}

} // namespace deimos
