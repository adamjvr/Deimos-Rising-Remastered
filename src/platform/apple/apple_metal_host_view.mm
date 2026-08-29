#include "deimos/apple_metal_host_view.hpp"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <TargetConditionals.h>

#if TARGET_OS_IPHONE
#import <UIKit/UIKit.h>
#else
#import <AppKit/AppKit.h>
#endif

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <utility>

namespace deimos {
namespace {

bool fail(std::string* error, const char* message) {
    if (error) *error = message;
    return false;
}

bool require_main_thread(std::string* error) {
    if (![NSThread isMainThread]) {
        return fail(error, "AppleMetalHostView must be used on the Apple main UI thread");
    }
    return true;
}

#if TARGET_OS_IPHONE

@interface DeimosMetalHostNativeView : UIView
@end

@implementation DeimosMetalHostNativeView
+ (Class)layerClass {
    return [CAMetalLayer class];
}
@end

using NativeView = DeimosMetalHostNativeView;

CAMetalLayer* metal_layer_for_view(NativeView* view) {
    return (CAMetalLayer*)view.layer;
}

double native_scale_for_view(NativeView* view) {
    const CGFloat scale = view.window != nil ? view.window.screen.scale : UIScreen.mainScreen.scale;
    return std::max(1.0, static_cast<double>(scale));
}

void set_native_frame(NativeView* view, double width, double height) {
    view.frame = CGRectMake(0.0, 0.0, width, height);
}

void configure_native_view(NativeView* view) {
    view.opaque = YES;
    view.backgroundColor = UIColor.blackColor;
    view.contentScaleFactor = static_cast<CGFloat>(native_scale_for_view(view));
}

#else

@interface DeimosMetalHostNativeView : NSView
@end

@implementation DeimosMetalHostNativeView
- (CALayer*)makeBackingLayer {
    return [CAMetalLayer layer];
}
- (BOOL)isFlipped {
    return YES;
}
@end

using NativeView = DeimosMetalHostNativeView;

CAMetalLayer* metal_layer_for_view(NativeView* view) {
    return (CAMetalLayer*)view.layer;
}

double native_scale_for_view(NativeView* view) {
    if (view.window != nil) {
        return std::max(1.0, static_cast<double>(view.window.backingScaleFactor));
    }
    NSScreen* screen = NSScreen.mainScreen;
    return screen == nil ? 1.0 : std::max(1.0, static_cast<double>(screen.backingScaleFactor));
}

void set_native_frame(NativeView* view, double width, double height) {
    [view setFrame:NSMakeRect(0.0, 0.0, width, height)];
}

void configure_native_view(NativeView* view) {
    view.wantsLayer = YES;
}

#endif

} // namespace

struct AppleMetalHostView::Impl {
    __strong NativeView* view = nil;
    __strong CAMetalLayer* layer = nil;
    std::unique_ptr<AppleMetalPresentationBackend> backend;
    ModernPresentationOptions options{};

    bool initialize(std::string* error) {
        if (!require_main_thread(error)) return false;
        if (view != nil && layer != nil && backend) return true;

        #if TARGET_OS_IPHONE
        view = [[NativeView alloc] initWithFrame:CGRectMake(0.0, 0.0, 640.0, 480.0)];
#else
        view = [[NativeView alloc] initWithFrame:NSMakeRect(0.0, 0.0, 640.0, 480.0)];
#endif
        if (view == nil) return fail(error, "failed to create native Apple host view");
        configure_native_view(view);

        layer = metal_layer_for_view(view);
        if (layer == nil || ![layer isKindOfClass:[CAMetalLayer class]]) {
            view = nil;
            return fail(error, "native Apple host view did not create a CAMetalLayer");
        }

        layer.opaque = YES;
        layer.framebufferOnly = YES;
        layer.pixelFormat = MTLPixelFormatBGRA8Unorm;

        backend = std::make_unique<AppleMetalPresentationBackend>((__bridge void*)layer);
        if (!sync_geometry(error)) {
            backend.reset();
            layer = nil;
            view = nil;
            return false;
        }
        return backend->ready(error);
    }

    bool sync_geometry(std::string* error) {
        if (!require_main_thread(error)) return false;
        if (view == nil || layer == nil) return fail(error, "Apple Metal host view is not initialized");

        const CGRect bounds = view.bounds;
        const double scale = native_scale_for_view(view);
        const double width_points = std::max(0.0, static_cast<double>(bounds.size.width));
        const double height_points = std::max(0.0, static_cast<double>(bounds.size.height));

#if TARGET_OS_IPHONE
        view.contentScaleFactor = static_cast<CGFloat>(scale);
#endif
        layer.contentsScale = static_cast<CGFloat>(scale);
        layer.frame = bounds;

        const auto width_px = static_cast<CGFloat>(std::max(0.0, std::round(width_points * scale)));
        const auto height_px = static_cast<CGFloat>(std::max(0.0, std::round(height_points * scale)));
        layer.drawableSize = CGSizeMake(width_px, height_px);
        return true;
    }

    ModernDrawableSize drawable_size() const noexcept {
        if (layer == nil) return {};
        const CGSize size = layer.drawableSize;
        return {
            static_cast<int>(std::llround(size.width)),
            static_cast<int>(std::llround(size.height)),
        };
    }
};

AppleMetalHostView::AppleMetalHostView()
    : impl_(std::make_unique<Impl>()) {}

AppleMetalHostView::~AppleMetalHostView() = default;
AppleMetalHostView::AppleMetalHostView(AppleMetalHostView&&) noexcept = default;
AppleMetalHostView& AppleMetalHostView::operator=(AppleMetalHostView&&) noexcept = default;

bool AppleMetalHostView::initialize(std::string* error) {
    return impl_->initialize(error);
}

void* AppleMetalHostView::native_view_handle() const noexcept {
    return impl_->view == nil ? nullptr : (__bridge void*)impl_->view;
}

void* AppleMetalHostView::metal_layer_handle() const noexcept {
    return impl_->layer == nil ? nullptr : (__bridge void*)impl_->layer;
}

bool AppleMetalHostView::set_size_points(
    double width_points,
    double height_points,
    std::string* error) {
    if (!require_main_thread(error)) return false;
    if (width_points <= 0.0 || height_points <= 0.0 ||
        !std::isfinite(width_points) || !std::isfinite(height_points)) {
        return fail(error, "Apple Metal host view point size must be finite and positive");
    }
    if (!impl_->initialize(error)) return false;
    set_native_frame(impl_->view, width_points, height_points);
    return impl_->sync_geometry(error);
}

bool AppleMetalHostView::sync_drawable_geometry(std::string* error) {
    if (!impl_->initialize(error)) return false;
    return impl_->sync_geometry(error);
}

ModernDrawableSize AppleMetalHostView::drawable_size() const noexcept {
    return impl_->drawable_size();
}

void AppleMetalHostView::set_presentation_options(ModernPresentationOptions options) noexcept {
    impl_->options = options;
}

const ModernPresentationOptions& AppleMetalHostView::presentation_options() const noexcept {
    return impl_->options;
}

bool AppleMetalHostView::present(
    const LegacyRasterSurface& canonical_display,
    const LegacyPresentationConfig& legacy_config,
    ModernPresentationResult& result,
    std::string* error) {
    result = {};
    if (!require_main_thread(error)) return false;
    if (!impl_->initialize(error)) return false;
    if (!impl_->sync_geometry(error)) return false;

    const ModernDrawableSize drawable = impl_->drawable_size();
    if (drawable.width <= 0 || drawable.height <= 0) {
        return fail(error, "Apple Metal host view has no drawable pixels");
    }

    return present_modern_frame(
        canonical_display,
        legacy_config,
        drawable,
        impl_->options,
        *impl_->backend,
        result,
        error);
}

bool AppleMetalHostView::ready(std::string* error) {
    if (!require_main_thread(error)) return false;
    if (!impl_->initialize(error)) return false;
    return impl_->backend->ready(error);
}

} // namespace deimos
