#pragma once

#include "deimos/presentation_runtime.hpp"
#include "deimos/render_backend.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace deimos {

// First host-facing presentation seam for the remaster. The deterministic
// renderer remains xRGB1555 and produces the exact canonical 640x480 frame;
// only this layer converts that completed frame for a modern native backend.
enum class ModernScalingMode : std::uint8_t {
    AspectFit = 0,
    IntegerFit = 1,
    Stretch = 2,
};

enum class ModernSamplingMode : std::uint8_t {
    Nearest = 0,
    Linear = 1,
};

struct ModernDrawableSize {
    int width = 0;
    int height = 0;
    constexpr bool operator==(const ModernDrawableSize&) const = default;
};

struct ModernViewport {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    constexpr bool operator==(const ModernViewport&) const = default;
};

struct ModernPresentationOptions {
    ModernScalingMode scaling = ModernScalingMode::AspectFit;
    ModernSamplingMode sampling = ModernSamplingMode::Nearest;
    std::array<std::uint8_t, 4> clear_rgba{{0, 0, 0, 255}};
};

// Stable, backend-neutral upload packet. Pixels are tightly packed RGBA8888,
// top-to-bottom, left-to-right. The viewport is expressed in drawable pixels.
struct ModernPresentationFrame {
    int source_width = 0;
    int source_height = 0;
    int row_bytes = 0;
    ModernDrawableSize drawable{};
    ModernViewport viewport{};
    ModernSamplingMode sampling = ModernSamplingMode::Nearest;
    std::array<std::uint8_t, 4> clear_rgba{{0, 0, 0, 255}};
    std::vector<std::uint8_t> rgba8888;

    [[nodiscard]] bool valid() const {
        return source_width > 0 && source_height > 0 &&
               row_bytes == source_width * 4 &&
               rgba8888.size() == static_cast<std::size_t>(row_bytes) *
                                      static_cast<std::size_t>(source_height) &&
               drawable.width > 0 && drawable.height > 0 &&
               viewport.x >= 0 && viewport.y >= 0 &&
               viewport.width > 0 && viewport.height > 0 &&
               viewport.x + viewport.width <= drawable.width &&
               viewport.y + viewport.height <= drawable.height;
    }
};

// Native implementations (Metal, Vulkan, D3D, etc.) implement only this
// interface. They receive an already completed canonical frame and therefore
// cannot perturb simulation or legacy raster determinism.
class ModernPresentationBackend {
public:
    virtual ~ModernPresentationBackend() = default;
    [[nodiscard]] virtual bool present(
        const ModernPresentationFrame& frame,
        std::string* error = nullptr) = 0;
};

struct ModernPresentationResult {
    bool converted = false;
    bool submitted = false;
    ModernViewport viewport{};
    std::size_t upload_bytes = 0;
};

// Expand one xRGB1555 pixel to RGBA8888. Bit 15 is ignored, matching the
// canonical renderer's 15-bit color semantics. 5-bit channels are expanded by
// bit replication so 0 and 31 map exactly to 0 and 255.
[[nodiscard]] constexpr std::array<std::uint8_t, 4> expand_xrgb1555_to_rgba8888(
    std::uint16_t pixel) {
    const auto expand5 = [](std::uint8_t v) constexpr -> std::uint8_t {
        return static_cast<std::uint8_t>((v << 3) | (v >> 2));
    };
    return {{
        expand5(static_cast<std::uint8_t>((pixel >> 10) & 31)),
        expand5(static_cast<std::uint8_t>((pixel >> 5) & 31)),
        expand5(static_cast<std::uint8_t>(pixel & 31)),
        255,
    }};
}

// Calculate the host drawable viewport without touching source pixels.
[[nodiscard]] bool plan_modern_viewport(
    int source_width,
    int source_height,
    ModernDrawableSize drawable,
    ModernScalingMode scaling,
    ModernViewport& viewport,
    std::string* error = nullptr);

// Convert a completed canonical legacy display frame to the modern upload
// packet. This deliberately requires the exact minimum legacy frame size
// (normally 640x480): callers should not feed an already host-centered legacy
// display into this layer and then scale it a second time.
[[nodiscard]] bool build_modern_presentation_frame(
    const LegacyRasterSurface& canonical_display,
    const LegacyPresentationConfig& legacy_config,
    ModernDrawableSize drawable,
    const ModernPresentationOptions& options,
    ModernPresentationFrame& frame,
    std::string* error = nullptr);


// Dependency-free CPU oracle for the host-facing layer. It clears the entire
// drawable and nearest-neighbour scales the RGBA upload into frame.viewport.
// Linear filtering is deliberately not an oracle because GPU sampler details
// differ across APIs; native backends may still request/use Linear visually.
[[nodiscard]] bool rasterize_modern_presentation_reference(
    const ModernPresentationFrame& frame,
    std::vector<std::uint8_t>& drawable_rgba8888,
    std::string* error = nullptr);

// Convenience boundary used by future native apps: build the immutable upload
// packet and hand it to the selected platform backend.
[[nodiscard]] bool present_modern_frame(
    const LegacyRasterSurface& canonical_display,
    const LegacyPresentationConfig& legacy_config,
    ModernDrawableSize drawable,
    const ModernPresentationOptions& options,
    ModernPresentationBackend& backend,
    ModernPresentationResult& result,
    std::string* error = nullptr);

} // namespace deimos
