#pragma once

#include "deimos/apple_metal_presentation_backend.hpp"
#include "deimos/modern_presentation_runtime.hpp"
#include "deimos/presentation_runtime.hpp"
#include "deimos/render_backend.hpp"

#include <memory>
#include <string>

namespace deimos {

// Minimal native Apple view host for the modern presentation seam.
//
// The implementation creates and owns an NSView on macOS or UIView on iPadOS,
// with a CAMetalLayer as its backing layer. The public interface remains plain
// C++ so application/runtime code does not need to import AppKit/UIKit/Metal.
//
// All view/layer operations must be performed on Apple's main UI thread.
class AppleMetalHostView final {
public:
    AppleMetalHostView();
    ~AppleMetalHostView();

    AppleMetalHostView(AppleMetalHostView&&) noexcept;
    AppleMetalHostView& operator=(AppleMetalHostView&&) noexcept;

    AppleMetalHostView(const AppleMetalHostView&) = delete;
    AppleMetalHostView& operator=(const AppleMetalHostView&) = delete;

    // Creates the native view and Metal backing layer. Safe to call more than
    // once; subsequent successful calls leave the existing view attached.
    [[nodiscard]] bool initialize(std::string* error = nullptr);

    // NSView* on macOS, UIView* on iPadOS. The pointer is borrowed; this C++
    // object retains/owns the native view for its lifetime.
    [[nodiscard]] void* native_view_handle() const noexcept;

    // CAMetalLayer* backing the native view. The pointer is borrowed.
    [[nodiscard]] void* metal_layer_handle() const noexcept;

    // Set the native view's logical size in points. Origin remains (0,0).
    // This also synchronizes CAMetalLayer.drawableSize in physical pixels.
    [[nodiscard]] bool set_size_points(
        double width_points,
        double height_points,
        std::string* error = nullptr);

    // Re-read the native view bounds and current Retina/device scale, then
    // update CAMetalLayer.frame/contentsScale/drawableSize. Hosts should call
    // this after attaching the view to a window or after native layout changes.
    [[nodiscard]] bool sync_drawable_geometry(std::string* error = nullptr);

    [[nodiscard]] ModernDrawableSize drawable_size() const noexcept;

    void set_presentation_options(ModernPresentationOptions options) noexcept;
    [[nodiscard]] const ModernPresentationOptions& presentation_options() const noexcept;

    // Present an already completed canonical legacy display surface through
    // the backend-neutral bridge and Metal. canonical_display should be the
    // recovered 640x480 xRGB1555 display frame, not an already host-scaled
    // surface.
    [[nodiscard]] bool present(
        const LegacyRasterSurface& canonical_display,
        const LegacyPresentationConfig& legacy_config,
        ModernPresentationResult& result,
        std::string* error = nullptr);

    // Diagnostic readiness check for host startup.
    [[nodiscard]] bool ready(std::string* error = nullptr);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace deimos
