#include "deimos/modern_presentation_runtime.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace deimos {
namespace {

bool fail(std::string* error, const char* message) {
    if (error) *error = message;
    return false;
}

bool aspect_fit(
    int source_width,
    int source_height,
    ModernDrawableSize drawable,
    ModernViewport& viewport) {
    const std::int64_t lhs = static_cast<std::int64_t>(drawable.width) * source_height;
    const std::int64_t rhs = static_cast<std::int64_t>(drawable.height) * source_width;

    int width = 0;
    int height = 0;
    if (lhs <= rhs) {
        width = drawable.width;
        height = static_cast<int>((static_cast<std::int64_t>(drawable.width) * source_height) /
                                  source_width);
    } else {
        height = drawable.height;
        width = static_cast<int>((static_cast<std::int64_t>(drawable.height) * source_width) /
                                 source_height);
    }

    width = std::max(width, 1);
    height = std::max(height, 1);
    viewport = {
        (drawable.width - width) / 2,
        (drawable.height - height) / 2,
        width,
        height,
    };
    return true;
}

} // namespace

bool plan_modern_viewport(
    int source_width,
    int source_height,
    ModernDrawableSize drawable,
    ModernScalingMode scaling,
    ModernViewport& viewport,
    std::string* error) {
    viewport = {};
    if (source_width <= 0 || source_height <= 0)
        return fail(error, "modern presentation source dimensions must be positive");
    if (drawable.width <= 0 || drawable.height <= 0)
        return fail(error, "modern drawable dimensions must be positive");

    switch (scaling) {
    case ModernScalingMode::AspectFit:
        return aspect_fit(source_width, source_height, drawable, viewport);

    case ModernScalingMode::IntegerFit: {
        const int scale = std::min(drawable.width / source_width, drawable.height / source_height);
        if (scale <= 0)
            return aspect_fit(source_width, source_height, drawable, viewport);
        const int width = source_width * scale;
        const int height = source_height * scale;
        viewport = {
            (drawable.width - width) / 2,
            (drawable.height - height) / 2,
            width,
            height,
        };
        return true;
    }

    case ModernScalingMode::Stretch:
        viewport = {0, 0, drawable.width, drawable.height};
        return true;
    }

    return fail(error, "unknown modern scaling mode");
}

bool build_modern_presentation_frame(
    const LegacyRasterSurface& canonical_display,
    const LegacyPresentationConfig& legacy_config,
    ModernDrawableSize drawable,
    const ModernPresentationOptions& options,
    ModernPresentationFrame& frame,
    std::string* error) {
    frame = {};
    if (!canonical_display.valid())
        return fail(error, "canonical legacy display surface is invalid");
    if (canonical_display.width != legacy_config.min_screen_width ||
        canonical_display.height != legacy_config.min_screen_height)
        return fail(error, "modern bridge requires the exact canonical legacy display dimensions");
    if (legacy_config.min_screen_width <= 0 || legacy_config.min_screen_height <= 0)
        return fail(error, "legacy presentation dimensions must be positive");

    ModernViewport viewport;
    if (!plan_modern_viewport(
            canonical_display.width, canonical_display.height,
            drawable, options.scaling, viewport, error))
        return false;

    const std::size_t pixel_count = static_cast<std::size_t>(canonical_display.width) *
                                    static_cast<std::size_t>(canonical_display.height);
    if (pixel_count > std::numeric_limits<std::size_t>::max() / 4)
        return fail(error, "modern RGBA upload size overflow");

    frame.source_width = canonical_display.width;
    frame.source_height = canonical_display.height;
    frame.row_bytes = canonical_display.width * 4;
    frame.drawable = drawable;
    frame.viewport = viewport;
    frame.sampling = options.sampling;
    frame.clear_rgba = options.clear_rgba;
    frame.rgba8888.resize(pixel_count * 4);

    std::size_t out = 0;
    for (const std::uint16_t pixel : canonical_display.pixels) {
        const auto rgba = expand_xrgb1555_to_rgba8888(pixel);
        frame.rgba8888[out++] = rgba[0];
        frame.rgba8888[out++] = rgba[1];
        frame.rgba8888[out++] = rgba[2];
        frame.rgba8888[out++] = rgba[3];
    }

    return true;
}


bool rasterize_modern_presentation_reference(
    const ModernPresentationFrame& frame,
    std::vector<std::uint8_t>& drawable_rgba8888,
    std::string* error) {
    drawable_rgba8888.clear();
    if (!frame.valid()) return fail(error, "modern presentation frame is invalid");
    if (frame.sampling != ModernSamplingMode::Nearest)
        return fail(error, "reference presenter only defines nearest-neighbour sampling");

    const std::size_t target_pixels = static_cast<std::size_t>(frame.drawable.width) *
                                      static_cast<std::size_t>(frame.drawable.height);
    if (target_pixels > std::numeric_limits<std::size_t>::max() / 4)
        return fail(error, "modern reference drawable size overflow");
    drawable_rgba8888.resize(target_pixels * 4);

    for (std::size_t i = 0; i < target_pixels; ++i) {
        const std::size_t o = i * 4;
        drawable_rgba8888[o + 0] = frame.clear_rgba[0];
        drawable_rgba8888[o + 1] = frame.clear_rgba[1];
        drawable_rgba8888[o + 2] = frame.clear_rgba[2];
        drawable_rgba8888[o + 3] = frame.clear_rgba[3];
    }

    for (int y = 0; y < frame.viewport.height; ++y) {
        const int sy = (frame.source_height * y) / frame.viewport.height;
        const auto src_row = static_cast<std::size_t>(sy) * static_cast<std::size_t>(frame.row_bytes);
        const auto dst_y = frame.viewport.y + y;
        const auto dst_row = static_cast<std::size_t>(dst_y) *
                             static_cast<std::size_t>(frame.drawable.width) * 4u;
        for (int x = 0; x < frame.viewport.width; ++x) {
            const int sx = (frame.source_width * x) / frame.viewport.width;
            const auto si = src_row + static_cast<std::size_t>(sx) * 4u;
            const auto dst_x = frame.viewport.x + x;
            const auto di = dst_row + static_cast<std::size_t>(dst_x) * 4u;
            drawable_rgba8888[di + 0] = frame.rgba8888[si + 0];
            drawable_rgba8888[di + 1] = frame.rgba8888[si + 1];
            drawable_rgba8888[di + 2] = frame.rgba8888[si + 2];
            drawable_rgba8888[di + 3] = frame.rgba8888[si + 3];
        }
    }
    return true;
}

bool present_modern_frame(
    const LegacyRasterSurface& canonical_display,
    const LegacyPresentationConfig& legacy_config,
    ModernDrawableSize drawable,
    const ModernPresentationOptions& options,
    ModernPresentationBackend& backend,
    ModernPresentationResult& result,
    std::string* error) {
    result = {};
    ModernPresentationFrame frame;
    if (!build_modern_presentation_frame(
            canonical_display, legacy_config, drawable, options, frame, error))
        return false;

    result.converted = true;
    result.viewport = frame.viewport;
    result.upload_bytes = frame.rgba8888.size();
    if (!backend.present(frame, error)) return false;
    result.submitted = true;
    return true;
}

} // namespace deimos
