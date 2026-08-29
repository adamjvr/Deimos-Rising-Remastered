#include "deimos/apple_metal_host_view.hpp"

#import <Foundation/Foundation.h>
#import <TargetConditionals.h>

#if TARGET_OS_IPHONE
#import <UIKit/UIKit.h>
#else
#import <AppKit/AppKit.h>
#endif

#include <cstdint>
#include <memory>
#include <string>

namespace {

std::uint16_t rgb555(int r, int g, int b) {
    return static_cast<std::uint16_t>(((r & 31) << 10) | ((g & 31) << 5) | (b & 31));
}

deimos::LegacyRasterSurface make_smoke_frame() {
    constexpr int width = 640;
    constexpr int height = 480;
    deimos::LegacyRasterSurface frame(width, height, 0);

    // Exercise the recovered minimum-frame geometry visibly:
    // 32 border + 416 gameplay + 160 score bar + 32 border.
    for (int y = 0; y < height; ++y) {
        for (int x = 32; x < 448; ++x) {
            const bool checker = (((x - 32) / 32) + (y / 32)) & 1;
            const int ramp = (y * 31) / (height - 1);
            frame.pixels[static_cast<std::size_t>(y) * width + x] =
                checker ? rgb555(3, 10 + ramp / 2, 31) : rgb555(2 + ramp / 3, 24, 8);
        }
        for (int x = 448; x < 608; ++x) {
            const int local = x - 448;
            const int band = local / 20;
            const std::uint16_t colors[8] = {
                rgb555(31, 5, 5), rgb555(31, 18, 3), rgb555(31, 31, 4), rgb555(5, 31, 8),
                rgb555(4, 24, 31), rgb555(5, 8, 31), rgb555(18, 5, 31), rgb555(31, 5, 24),
            };
            frame.pixels[static_cast<std::size_t>(y) * width + x] = colors[band & 7];
        }
    }

    // White one-pixel guides at the canonical region boundaries make viewport
    // orientation/scaling mistakes obvious in screenshots.
    for (int y = 0; y < height; ++y) {
        for (int x : {31, 32, 447, 448, 607, 608}) {
            frame.pixels[static_cast<std::size_t>(y) * width + x] = rgb555(31, 31, 31);
        }
    }
    return frame;
}

bool present_host(deimos::AppleMetalHostView& host, const deimos::LegacyRasterSurface& frame) {
    deimos::ModernPresentationResult result{};
    std::string error;
    if (!host.present(frame, deimos::LegacyPresentationConfig{}, result, &error)) {
        NSLog(@"Deimos host smoke present failed: %s", error.c_str());
        return false;
    }
    return true;
}

} // namespace

#if TARGET_OS_IPHONE

@interface DeimosSmokeViewController : UIViewController
@end

@implementation DeimosSmokeViewController {
    std::unique_ptr<deimos::AppleMetalHostView> _host;
    deimos::LegacyRasterSurface _frame;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = UIColor.blackColor;

    _host = std::make_unique<deimos::AppleMetalHostView>();
    _frame = make_smoke_frame();
    std::string error;
    if (!_host->initialize(&error)) {
        NSLog(@"Deimos Apple host initialization failed: %s", error.c_str());
        return;
    }

    deimos::ModernPresentationOptions options{};
    options.scaling = deimos::ModernScalingMode::AspectFit;
    options.sampling = deimos::ModernSamplingMode::Nearest;
    _host->set_presentation_options(options);

    UIView* metalView = (__bridge UIView*)_host->native_view_handle();
    metalView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    [self.view addSubview:metalView];
}

- (void)viewDidLayoutSubviews {
    [super viewDidLayoutSubviews];
    if (!_host) return;
    const CGSize size = self.view.bounds.size;
    std::string error;
    if (!_host->set_size_points(size.width, size.height, &error)) {
        NSLog(@"Deimos Apple host resize failed: %s", error.c_str());
        return;
    }
    (void)present_host(*_host, _frame);
}

- (BOOL)prefersStatusBarHidden {
    return YES;
}
@end

@interface DeimosSmokeAppDelegate : UIResponder <UIApplicationDelegate>
@property(nonatomic, strong) UIWindow* window;
@end

@implementation DeimosSmokeAppDelegate
- (BOOL)application:(UIApplication*)application didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
    (void)application;
    (void)launchOptions;
    self.window = [[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
    self.window.rootViewController = [[DeimosSmokeViewController alloc] init];
    [self.window makeKeyAndVisible];
    return YES;
}
@end

int main(int argc, char* argv[]) {
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([DeimosSmokeAppDelegate class]));
    }
}

#else

@interface DeimosSmokeAppDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate>
@end

@implementation DeimosSmokeAppDelegate {
    NSWindow* _window;
    std::unique_ptr<deimos::AppleMetalHostView> _host;
    deimos::LegacyRasterSurface _frame;
}

- (void)applicationDidFinishLaunching:(NSNotification*)notification {
    (void)notification;
    _host = std::make_unique<deimos::AppleMetalHostView>();
    _frame = make_smoke_frame();

    std::string error;
    if (!_host->initialize(&error)) {
        NSLog(@"Deimos Apple host initialization failed: %s", error.c_str());
        [NSApp terminate:nil];
        return;
    }

    deimos::ModernPresentationOptions options{};
    options.scaling = deimos::ModernScalingMode::AspectFit;
    options.sampling = deimos::ModernSamplingMode::Nearest;
    _host->set_presentation_options(options);

    const NSRect frameRect = NSMakeRect(0.0, 0.0, 960.0, 720.0);
    _window = [[NSWindow alloc]
        initWithContentRect:frameRect
                  styleMask:(NSWindowStyleMaskTitled |
                             NSWindowStyleMaskClosable |
                             NSWindowStyleMaskMiniaturizable |
                             NSWindowStyleMaskResizable)
                    backing:NSBackingStoreBuffered
                      defer:NO];
    _window.title = @"Deimos Rising - Metal Host Smoke";
    _window.delegate = self;

    NSView* metalView = (__bridge NSView*)_host->native_view_handle();
    metalView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    _window.contentView = metalView;
    [_window center];
    [_window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];

    (void)_host->sync_drawable_geometry(&error);
    (void)present_host(*_host, _frame);
}

- (void)windowDidResize:(NSNotification*)notification {
    (void)notification;
    if (!_host) return;
    std::string error;
    if (!_host->sync_drawable_geometry(&error)) {
        NSLog(@"Deimos Apple host resize failed: %s", error.c_str());
        return;
    }
    (void)present_host(*_host, _frame);
}

- (void)windowDidChangeBackingProperties:(NSNotification*)notification {
    (void)notification;
    [self windowDidResize:nil];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
    (void)sender;
    return YES;
}
@end

int main(int argc, const char* argv[]) {
    (void)argc;
    (void)argv;
    @autoreleasepool {
        NSApplication* app = [NSApplication sharedApplication];
        app.activationPolicy = NSApplicationActivationPolicyRegular;
        DeimosSmokeAppDelegate* delegate = [[DeimosSmokeAppDelegate alloc] init];
        app.delegate = delegate;
        [app run];
    }
    return 0;
}

#endif
