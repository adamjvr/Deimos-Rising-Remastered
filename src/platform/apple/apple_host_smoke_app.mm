#include "deimos/apple_metal_host_view.hpp"
#include "deimos/original_game_frame_preview.hpp"

#import <Foundation/Foundation.h>
#import <TargetConditionals.h>

#if TARGET_OS_IPHONE
#import <UIKit/UIKit.h>
#else
#import <AppKit/AppKit.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {


std::filesystem::path ns_path(NSString* value) {
    if (value == nil) return {};
    return std::filesystem::path(value.fileSystemRepresentation);
}

void add_search_root_candidates(
    std::vector<std::filesystem::path>& candidates,
    std::filesystem::path root) {
    for (int depth = 0; depth < 10 && !root.empty(); ++depth) {
        candidates.push_back(root / "reference" / "DR-EVID-002" / "canonical" / "Paks");
        candidates.push_back(root / "Paks");
        candidates.push_back(root);
        const auto parent = root.parent_path();
        if (parent == root) break;
        root = parent;
    }
}

std::optional<std::filesystem::path> find_original_pak_directory() {
    std::vector<std::filesystem::path> candidates;
    if (const char* env = std::getenv("DEIMOS_ORIGINAL_PAK_DIR"); env && *env) {
        candidates.emplace_back(env);
    }

    NSString* resourcePath = NSBundle.mainBundle.resourcePath;
    if (resourcePath != nil) {
        const auto resources = ns_path(resourcePath);
        candidates.push_back(resources / "Paks");
        candidates.push_back(resources);
        add_search_root_candidates(candidates, resources);
    }

#if !TARGET_OS_IPHONE
    std::error_code ec;
    const auto cwd = std::filesystem::current_path(ec);
    if (!ec) add_search_root_candidates(candidates, cwd);
    add_search_root_candidates(candidates, ns_path(NSBundle.mainBundle.bundlePath));
#endif

    for (const auto& candidate : candidates) {
        if (deimos::original_game_pak_directory_valid(candidate)) return candidate;
    }
    return std::nullopt;
}


std::uint16_t rgb555(int r, int g, int b) {
    return static_cast<std::uint16_t>(((r & 31) << 10) | ((g & 31) << 5) | (b & 31));
}

deimos::LegacyRasterSurface make_smoke_frame() {
    constexpr int width = 640;
    constexpr int height = 480;
    deimos::LegacyRasterSurface frame(width, height, 0);

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
    for (int y = 0; y < height; ++y) {
        for (int x : {31, 32, 447, 448, 607, 608}) {
            frame.pixels[static_cast<std::size_t>(y) * width + x] = rgb555(31, 31, 31);
        }
    }
    return frame;
}

class SmokeFrameSource {
public:
    bool initialize(std::string& description) {
        const auto pak_dir = find_original_pak_directory();
        if (pak_dir) {
            std::string error;
            auto loaded = deimos::OriginalGameFramePreview::load(
                *pak_dir, {{'l','e','0','1'}}, 0, &error);
            if (loaded) {
                preview_ = std::make_unique<deimos::OriginalGameFramePreview>(std::move(*loaded));
                deimos::LegacyGameplayFrameResult frame_result{};
                if (preview_->render(frame_, &frame_result, &error)) {
                    const auto& info = preview_->info();
                    description = info.level_name + " [" + info.level_id.str() + "] / " + info.player_name;
                    fps_ = info.fps_max_rate > 0.0f ? info.fps_max_rate : 30.0f;
                    NSLog(@"Deimos live original-data frame: PAKs=%s level=%s background=%s playerFace=%s groups=%zu fps=%.2f",
                          pak_dir->string().c_str(), info.level_name.c_str(), info.background_id.str().c_str(),
                          info.player_face.str().c_str(), info.loaded_sprite_groups, fps_);
                    return true;
                }
            }
            NSLog(@"Deimos original-data live source unavailable: %s", error.c_str());
            preview_.reset();
        } else {
            NSLog(@"Deimos original-data live source unavailable: Game.pak + Interface.pak not found");
        }

        description = "Diagnostic 32+416+160+32 frame";
        frame_ = make_smoke_frame();
        fps_ = 0.0f;
        return true;
    }

    bool advance() {
        if (!preview_) return true;
        const auto tick = preview_->tick();
        deimos::LegacyGameplayFrameResult frame_result{};
        std::string error;
        if (!preview_->render(frame_, &frame_result, &error)) {
            NSLog(@"Deimos live frame render failed at tick %llu: %s",
                  static_cast<unsigned long long>(tick.tick_index), error.c_str());
            return false;
        }
        return true;
    }

    [[nodiscard]] bool live() const noexcept { return preview_ != nullptr; }
    [[nodiscard]] double fps() const noexcept { return fps_; }
    [[nodiscard]] const deimos::LegacyRasterSurface& frame() const noexcept { return frame_; }

private:
    std::unique_ptr<deimos::OriginalGameFramePreview> preview_;
    deimos::LegacyRasterSurface frame_{};
    double fps_ = 0.0;
};

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
    std::unique_ptr<SmokeFrameSource> _source;
    CADisplayLink* _displayLink;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = UIColor.blackColor;

    _host = std::make_unique<deimos::AppleMetalHostView>();
    _source = std::make_unique<SmokeFrameSource>();
    std::string frameDescription;
    (void)_source->initialize(frameDescription);
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

    if (_source->live()) {
        _displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(deimosDisplayTick:)];
        _displayLink.preferredFramesPerSecond = std::max<NSInteger>(
            1, static_cast<NSInteger>(std::llround(_source->fps())));
        [_displayLink addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
    }
}

- (void)deimosDisplayTick:(CADisplayLink*)displayLink {
    (void)displayLink;
    if (!_host || !_source || !_source->live()) return;
    if (!_source->advance()) return;
    (void)present_host(*_host, _source->frame());
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
    (void)present_host(*_host, _source->frame());
}

- (void)dealloc {
    [_displayLink invalidate];
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
    NSTimer* _timer;
    std::unique_ptr<deimos::AppleMetalHostView> _host;
    std::unique_ptr<SmokeFrameSource> _source;
}

- (void)applicationDidFinishLaunching:(NSNotification*)notification {
    (void)notification;
    _host = std::make_unique<deimos::AppleMetalHostView>();
    _source = std::make_unique<SmokeFrameSource>();
    std::string frameDescription;
    (void)_source->initialize(frameDescription);

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
    NSString* detail = [NSString stringWithUTF8String:frameDescription.c_str()];
    _window.title = [NSString stringWithFormat:@"Deimos Rising - Metal Host Smoke - %@", detail];
    _window.delegate = self;

    NSView* metalView = (__bridge NSView*)_host->native_view_handle();
    metalView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    _window.contentView = metalView;
    [_window center];
    [_window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];

    (void)_host->sync_drawable_geometry(&error);
    (void)present_host(*_host, _source->frame());

    if (_source->live()) {
        const NSTimeInterval interval = 1.0 / std::max(1.0, _source->fps());
        _timer = [NSTimer timerWithTimeInterval:interval
                                        target:self
                                      selector:@selector(deimosLiveTick:)
                                      userInfo:nil
                                       repeats:YES];
        [NSRunLoop.mainRunLoop addTimer:_timer forMode:NSRunLoopCommonModes];
    }
}

- (void)deimosLiveTick:(NSTimer*)timer {
    (void)timer;
    if (!_host || !_source || !_source->live()) return;
    if (!_source->advance()) return;
    (void)present_host(*_host, _source->frame());
}

- (void)windowDidResize:(NSNotification*)notification {
    (void)notification;
    if (!_host) return;
    std::string error;
    if (!_host->sync_drawable_geometry(&error)) {
        NSLog(@"Deimos Apple host resize failed: %s", error.c_str());
        return;
    }
    (void)present_host(*_host, _source->frame());
}

- (void)windowDidChangeBackingProperties:(NSNotification*)notification {
    (void)notification;
    [self windowDidResize:nil];
}

- (void)windowWillClose:(NSNotification*)notification {
    (void)notification;
    [_timer invalidate];
    _timer = nil;
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
