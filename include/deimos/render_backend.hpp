#pragma once

#include "deimos/resource_id.hpp"
#include "deimos/sprite_resource.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace deimos {

struct LegacyRasterRect {
    int top = 0;
    int left = 0;
    int bottom = 0;
    int right = 0;

    [[nodiscard]] bool empty() const { return right <= left || bottom <= top; }
};

struct LegacyRasterSurface {
    int width = 0;
    int height = 0;
    std::vector<std::uint16_t> pixels;

    LegacyRasterSurface() = default;
    LegacyRasterSurface(int w, int h, std::uint16_t fill = 0)
        : width(w), height(h), pixels(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), fill) {}

    [[nodiscard]] bool valid() const {
        return width >= 0 && height >= 0 &&
               pixels.size() == static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    }
    [[nodiscard]] LegacyRasterRect bounds() const { return {0, 0, height, width}; }
};

// Exact low request bits consumed by 0x19570.
inline constexpr std::uint32_t kLegacyRenderOverallTransparency = 0x1;
inline constexpr std::uint32_t kLegacyRenderShadow = 0x2;
inline constexpr std::uint32_t kLegacyRenderSolidColor = 0x4;
inline constexpr std::uint32_t kLegacyRenderTerrainTarget = 0x8;

// Clean typed counterpart of the original 76-byte render request. Fields
// through effect_color correspond directly to the recovered record offsets;
// special_rect/special_color represent the dormant 'COST' rectangle path.
struct LegacyRasterRequest {
    const LegacySpriteFrameMetadata* frame = nullptr;
    int center_x = 0;
    int center_y = 0;
    FourCC sprite_face{};
    int sprite_frame = 0;
    std::uint32_t flags = 0;
    float scale = 1.0f;
    int effect_amount_0_to_32 = 0;
    LegacyRasterRect clip{};
    std::uint8_t numeric_layer = 0;
    bool immediate = false;
    std::uint16_t effect_color = 0;

    LegacyRasterRect special_rect{};
    std::uint16_t special_color = 0;
};

struct LegacyRasterConfig {
    // 0x1F040 menu/debug toggle: "Sprite Alpha Drawing Enabled/Disabled".
    // The scaled mirror at live global -25022 is updated by the same toggle.
    bool alpha_drawing_enabled = true;
    // 0x1AFC0 toggle: "Sprite FX Enabled/Disabled". When disabled, 0x18A40
    // submits a copied request with scale=1.0 and effect amount=0.
    bool sprite_fx_enabled = true;
};

enum class LegacyRasterResult {
    drawn,
    skipped,
    invalid_surface,
    missing_frame,
};

// Exact xRGB1555 arithmetic used by the legacy compositor. transparency is
// destination weight in the old 0..32 domain: 0 copies src, 32 preserves dst.
[[nodiscard]] std::uint16_t legacy_blend_rgb555(
    std::uint16_t dst,
    std::uint16_t src,
    int transparency_0_to_32);

// Shadow mode does not draw source color; it darkens the existing destination
// pixel by a 0..32 retained-brightness factor.
[[nodiscard]] std::uint16_t legacy_scale_rgb555(
    std::uint16_t pixel,
    int factor_0_to_32);

// 0x19570 compositor, including scale==1 direct/clipped families and the
// 0x1A6F0/0x1AA90 nearest-neighbour scaled families. Source addressing uses
// the original integer-ratio mapping from the complete scaled destination rect.
[[nodiscard]] LegacyRasterResult rasterize_legacy_request(
    const LegacyRasterRequest& request,
    LegacyRasterSurface& destination,
    LegacyRasterConfig config = {});

// 0x1EC80 special solid-rectangle path used when sprite_face == 'COST'.
[[nodiscard]] LegacyRasterResult rasterize_legacy_solid_rect(
    LegacyRasterSurface& destination,
    LegacyRasterRect rect,
    LegacyRasterRect clip,
    std::uint16_t color,
    int transparency_0_to_32);

class LegacyRenderQueue {
public:
    void enqueue(LegacyRasterRequest request);
    [[nodiscard]] std::size_t size(std::uint8_t layer) const;

    // 0x1A650: draw every matching queued record. Layers 0 and 1 are one-shot
    // and have their face replaced by 'none' after the flush; other layers
    // remain resident until explicit queue reset, matching the original list.
    std::vector<LegacyRasterResult> flush_layer(
        std::uint8_t layer,
        LegacyRasterSurface& main_surface,
        LegacyRasterSurface& terrain_surface,
        LegacyRasterConfig config = {});

    // 0x18B20 flush groups: 0 -> layers 0..1, 1 -> 2..5, 2 -> 6..15.
    std::vector<LegacyRasterResult> flush_group(
        std::uint8_t group,
        LegacyRasterSurface& main_surface,
        LegacyRasterSurface& terrain_surface,
        LegacyRasterConfig config = {});

    void clear();

private:
    std::array<std::vector<LegacyRasterRequest>, 16> layers_{};
};

// 0x18A40/0x19570 submission split: immediate requests rasterize now; ordinary
// requests are copied into the per-layer queue and rasterized by a later flush.
[[nodiscard]] LegacyRasterResult submit_legacy_render_request(
    LegacyRasterRequest request,
    LegacyRenderQueue& queue,
    LegacyRasterSurface& main_surface,
    LegacyRasterSurface& terrain_surface,
    LegacyRasterConfig config = {});

} // namespace deimos
