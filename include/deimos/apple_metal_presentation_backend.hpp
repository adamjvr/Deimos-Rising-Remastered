#pragma once

#include "deimos/modern_presentation_runtime.hpp"

#include <memory>
#include <string>

namespace deimos {

// Native Apple host adapter for the backend-neutral modern presentation seam.
//
// The handle passed to this class must be a CAMetalLayer*. It is intentionally
// typed as void* here so clean C++ application code can own the interface
// without importing Objective-C/QuartzCore declarations. The implementation
// lives in Objective-C++ and retains the layer for the backend lifetime.
//
// This class is built only on Apple platforms as target `deimos_metal_backend`.
class AppleMetalPresentationBackend final : public ModernPresentationBackend {
public:
    explicit AppleMetalPresentationBackend(void* cametal_layer = nullptr);
    ~AppleMetalPresentationBackend() override;

    AppleMetalPresentationBackend(AppleMetalPresentationBackend&&) noexcept;
    AppleMetalPresentationBackend& operator=(AppleMetalPresentationBackend&&) noexcept;

    AppleMetalPresentationBackend(const AppleMetalPresentationBackend&) = delete;
    AppleMetalPresentationBackend& operator=(const AppleMetalPresentationBackend&) = delete;

    // Replace the target CAMetalLayer. Passing nullptr detaches the backend.
    void set_layer(void* cametal_layer);
    [[nodiscard]] void* layer_handle() const noexcept;

    // Physical CAMetalLayer drawable size in pixels. A zero size means no
    // currently usable drawable and should not be submitted to the bridge.
    [[nodiscard]] ModernDrawableSize drawable_size() const noexcept;

    // Verifies that a layer/device/queue/pipeline are available. This performs
    // no draw and is suitable for host startup diagnostics.
    [[nodiscard]] bool ready(std::string* error = nullptr);

    // Uploads the immutable RGBA8888 canonical frame, clears the full drawable,
    // draws exactly one textured quad into frame.viewport, then presents the
    // CAMetalDrawable. No simulation or canonical raster state is touched.
    [[nodiscard]] bool present(
        const ModernPresentationFrame& frame,
        std::string* error = nullptr) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace deimos
