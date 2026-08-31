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
    enum class ControlDirection { Left, Right, Up, Down };
    enum class WeaponAction { FireAir, FireGround, SwitchAir };
    bool initialize(std::string& description) {
        const auto pak_dir = find_original_pak_directory();
        if (!pak_dir) {
            description = "Game.pak + Interface.pak not found";
            NSLog(@"Deimos playable host unavailable: %s", description.c_str());
            return false;
        }

        std::string error;
        auto loaded = deimos::OriginalGameFramePreview::load(
            *pak_dir, {{'l','e','0','1'}}, 0, &error);
        if (!loaded) {
            description = "original-data load failed: " + error;
            NSLog(@"Deimos playable host unavailable: %s", description.c_str());
            return false;
        }

        preview_ = std::make_unique<deimos::OriginalGameFramePreview>(std::move(*loaded));
        if (!preview_->enable_live_world(&error)) {
            description = "live-world bootstrap failed: " + error;
            NSLog(@"Deimos playable host unavailable: %s", description.c_str());
            preview_.reset();
            return false;
        }
        full_world_ = true;

        deimos::LegacyGameplayFrameResult frame_result{};
        if (!preview_->render(frame_, &frame_result, &error)) {
            description = "initial live render failed: " + error;
            NSLog(@"Deimos playable host unavailable: %s", description.c_str());
            preview_.reset();
            full_world_ = false;
            return false;
        }

        const auto& info = preview_->info();
        description = info.level_name + " [" + info.level_id.str() + "] / " + info.player_name +
            " / PLAYABLE WIP 7 / FRONT END";
        fps_ = info.fps_max_rate > 0.0f ? info.fps_max_rate : 30.0f;
        NSLog(@"Deimos playable original-data host: PAKs=%s level=%s background=%s playerFace=%s groups=%zu fps=%.2f liveObjects=%zu",
              pak_dir->string().c_str(), info.level_name.c_str(), info.background_id.str().c_str(),
              info.player_face.str().c_str(), info.loaded_sprite_groups, fps_,
              preview_->entity_world().active_member_count());
        if (const auto* weapon = preview_->selected_air_weapon()) {
            NSLog(@"Deimos selected air weapon: %s [%s]", weapon->name.c_str(), weapon->id.str().c_str());
        }
        if (const auto* weapon = preview_->selected_ground_weapon()) {
            NSLog(@"Deimos selected ground weapon: %s [%s]", weapon->name.c_str(), weapon->id.str().c_str());
        }
        return true;
    }

    bool restart(std::string& description) {
        preview_.reset();
        full_world_ = false;
        input_ = {};
        return initialize(description);
    }

    void set_control_direction(ControlDirection direction, bool pressed) {
        switch (direction) {
            case ControlDirection::Left: input_.movement.left = pressed; break;
            case ControlDirection::Right: input_.movement.right = pressed; break;
            case ControlDirection::Up: input_.movement.up = pressed; break;
            case ControlDirection::Down: input_.movement.down = pressed; break;
        }
    }

    void set_weapon_action(WeaponAction action, bool pressed) {
        switch (action) {
            case WeaponAction::FireAir: input_.weapons.fire_air = pressed; break;
            case WeaponAction::FireGround: input_.weapons.fire_ground = pressed; break;
            case WeaponAction::SwitchAir: input_.weapons.switch_air = pressed; break;
        }
    }

    void clear_control() noexcept { input_ = {}; }

    bool advance() {
        if (!preview_) return true;
        deimos::OriginalGameFrameTickResult tick{};
        std::string error;
        if (full_world_) {
            const auto live = preview_->tick_live(input_, &error);
            tick = live.frame;
            if (live.weapons.air_launched) {
                NSLog(@"Deimos AIR FIRE accepted at tick %llu: +%zu members",
                      static_cast<unsigned long long>(tick.tick_index), live.constructed_members);
            }
            if (live.weapons.ground_launched) {
                NSLog(@"Deimos GROUND FIRE accepted at tick %llu: +%zu members",
                      static_cast<unsigned long long>(tick.tick_index), live.constructed_members);
            }
            if (live.weapons.air_powerup_activated) {
                NSLog(@"Deimos AIR CHARGE activated at tick %llu",
                      static_cast<unsigned long long>(tick.tick_index));
            }
            if (live.weapons.air_powerup_released) {
                NSLog(@"Deimos AIR CHARGE released at tick %llu: power=%.1f%% queued=%zu",
                      static_cast<unsigned long long>(tick.tick_index),
                      live.weapons.air_power_percentage, live.weapons.powerup_requests.size());
            }
            if ((tick.tick_index % 300u) == 0u) {
                const auto& player = preview_->player_runtime();
                NSLog(@"Deimos runtime tick %llu: resident=%zu active=%zu groups=%zu particles=%zu/%zu score=%d lives=%d shield=%.1f power=%.1f money=%d",
                      static_cast<unsigned long long>(tick.tick_index),
                      preview_->entity_world().members().size(), live.active_entities,
                      preview_->entity_world().groups().size(), live.particle_systems,
                      live.active_particles, player.score, player.lives,
                      player.shield_percentage, live.weapons.air_power_percentage, player.money);
            }
            if (!error.empty()) {
                NSLog(@"Deimos live-world tick failed at tick %llu: %s",
                      static_cast<unsigned long long>(tick.tick_index), error.c_str());
                return false;
            }
        } else {
            tick = preview_->tick(input_.movement);
        }
        deimos::LegacyGameplayFrameResult frame_result{};
        if (!preview_->render(frame_, &frame_result, &error)) {
            NSLog(@"Deimos live frame render failed at tick %llu: %s",
                  static_cast<unsigned long long>(tick.tick_index), error.c_str());
            return false;
        }
        return true;
    }

    [[nodiscard]] bool live() const noexcept { return preview_ != nullptr && full_world_; }
    [[nodiscard]] double fps() const noexcept { return fps_; }
    [[nodiscard]] const deimos::LegacyRasterSurface& frame() const noexcept { return frame_; }

private:
    std::unique_ptr<deimos::OriginalGameFramePreview> preview_;
    deimos::LegacyRasterSurface frame_{};
    deimos::OriginalGameLiveInput input_{};
    bool full_world_ = false;
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

@interface DeimosGameWindow : NSWindow
@property(nonatomic, copy) void (^deimosKeyHandler)(NSEvent* event, BOOL pressed);
@end

@implementation DeimosGameWindow
- (void)keyDown:(NSEvent*)event {
    if (self.deimosKeyHandler != nil) self.deimosKeyHandler(event, YES);
    else [super keyDown:event];
}
- (void)keyUp:(NSEvent*)event {
    if (self.deimosKeyHandler != nil) self.deimosKeyHandler(event, NO);
    else [super keyUp:event];
}
- (void)flagsChanged:(NSEvent*)event {
    // Shift is delivered by AppKit as a modifier transition rather than an
    // ordinary keyDown:/keyUp: event. Treat the aggregate Shift state as the
    // semantic ground-fire button: releasing one Shift while the other remains
    // held correctly stays pressed, and releasing the final Shift releases it.
    if (self.deimosKeyHandler != nil) {
        NSEventModifierFlags mask = 0;
        switch (event.keyCode) {
            case 56: case 60: mask = NSEventModifierFlagShift; break;
            case 58: case 61: mask = NSEventModifierFlagOption; break;
            case 55: case 54: mask = NSEventModifierFlagCommand; break;
            default: break;
        }
        if (mask != 0) {
            const BOOL pressed = (event.modifierFlags & mask) != 0;
            self.deimosKeyHandler(event, pressed);
            return;
        }
    }
    [super flagsChanged:event];
}
@end

@interface DeimosSmokeAppDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate>
@end

@implementation DeimosSmokeAppDelegate {
    NSWindow* _window;
    NSTimer* _timer;
    std::unique_ptr<deimos::AppleMetalHostView> _host;
    std::unique_ptr<SmokeFrameSource> _source;
    BOOL _paused;
    NSString* _baseTitle;
}

- (void)installApplicationMenus {
    NSMenu* bar = [[NSMenu alloc] initWithTitle:@""];
    NSApp.mainMenu = bar;

    NSMenuItem* appRoot = [[NSMenuItem alloc] initWithTitle:@"Deimos Rising" action:nil keyEquivalent:@""];
    [bar addItem:appRoot];
    NSMenu* app = [[NSMenu alloc] initWithTitle:@"Deimos Rising"];
    appRoot.submenu = app;
    [app addItemWithTitle:@"About Deimos Rising" action:@selector(orderFrontStandardAboutPanel:) keyEquivalent:@""];
    [app addItem:[NSMenuItem separatorItem]];
    NSMenuItem* prefs = [app addItemWithTitle:@"Preferences…" action:@selector(showPreferences:) keyEquivalent:@","];
    prefs.target = self;
    [app addItem:[NSMenuItem separatorItem]];
    [app addItemWithTitle:@"Hide Deimos Rising" action:@selector(hide:) keyEquivalent:@"h"];
    [app addItem:[NSMenuItem separatorItem]];
    [app addItemWithTitle:@"Quit Deimos Rising" action:@selector(terminate:) keyEquivalent:@"q"];

    NSMenuItem* gameRoot = [[NSMenuItem alloc] initWithTitle:@"Game" action:nil keyEquivalent:@""];
    [bar addItem:gameRoot];
    NSMenu* game = [[NSMenu alloc] initWithTitle:@"Game"];
    gameRoot.submenu = game;
    NSMenuItem* pause = [game addItemWithTitle:@"Pause / Resume" action:@selector(togglePause:) keyEquivalent:@""];
    pause.target = self;
    NSMenuItem* restart = [game addItemWithTitle:@"Restart Level" action:@selector(restartLevel:) keyEquivalent:@"r"];
    restart.target = self;
    [game addItem:[NSMenuItem separatorItem]];
    NSMenuItem* controls = [game addItemWithTitle:@"Controls…" action:@selector(showControls:) keyEquivalent:@"k"];
    controls.target = self;

    NSMenuItem* viewRoot = [[NSMenuItem alloc] initWithTitle:@"View" action:nil keyEquivalent:@""];
    [bar addItem:viewRoot];
    NSMenu* view = [[NSMenu alloc] initWithTitle:@"View"];
    viewRoot.submenu = view;
    NSMenuItem* fs = [view addItemWithTitle:@"Toggle Full Screen" action:@selector(toggleFullScreen:) keyEquivalent:@"f"];
    fs.keyEquivalentModifierMask = NSEventModifierFlagControl | NSEventModifierFlagCommand;

    NSMenuItem* helpRoot = [[NSMenuItem alloc] initWithTitle:@"Help" action:nil keyEquivalent:@""];
    [bar addItem:helpRoot];
    NSMenu* help = [[NSMenu alloc] initWithTitle:@"Help"];
    helpRoot.submenu = help;
    NSMenuItem* quick = [help addItemWithTitle:@"Deimos Rising Controls" action:@selector(showControls:) keyEquivalent:@"?"];
    quick.target = self;
}

- (void)refreshTitle {
    if (!_window || !_baseTitle) return;
    _window.title = _paused ? [_baseTitle stringByAppendingString:@" — PAUSED"] : _baseTitle;
}

- (void)showPauseMenu {
    if (!_source || !_source->live()) return;
    _paused = YES;
    _source->clear_control();
    [self refreshTitle];

    while (_paused) {
        NSAlert* a = [[NSAlert alloc] init];
        a.messageText = @"Deimos Rising — Paused";
        a.informativeText = @"Choose an action. Escape also opens this menu during play.";
        [a addButtonWithTitle:@"Resume"];
        [a addButtonWithTitle:@"Controls…"];
        [a addButtonWithTitle:@"Preferences…"];
        [a addButtonWithTitle:@"Restart Level"];
        NSModalResponse r = [a runModal];
        if (r == NSAlertFirstButtonReturn) {
            _paused = NO;
        } else if (r == NSAlertSecondButtonReturn) {
            [self showControls:nil];
            _paused = YES;
        } else if (r == NSAlertThirdButtonReturn) {
            [self showPreferences:nil];
            _paused = YES;
        } else {
            [self restartLevel:nil];
            _paused = NO;
        }
        [self refreshTitle];
    }
}

- (void)showLaunchMenu {
    _paused = YES;
    [self refreshTitle];
    BOOL choosing = YES;
    while (choosing) {
        NSAlert* a = [[NSAlert alloc] init];
        a.messageText = @"Deimos Rising";
        a.informativeText = @"Kepler Massif — Level 1\n\nThe original 1.0.6 front end exposed game controls and preferences before play. This recovered host now does the same instead of dropping directly into combat.";
        [a addButtonWithTitle:@"Start Level"];
        [a addButtonWithTitle:@"Controls…"];
        [a addButtonWithTitle:@"Preferences…"];
        [a addButtonWithTitle:@"Quit"];
        NSModalResponse r = [a runModal];
        if (r == NSAlertFirstButtonReturn) {
            choosing = NO;
        } else if (r == NSAlertSecondButtonReturn) {
            [self showControls:nil];
            _paused = YES;
        } else if (r == NSAlertThirdButtonReturn) {
            [self showPreferences:nil];
            _paused = YES;
        } else {
            [NSApp terminate:nil];
            return;
        }
    }
    _paused = NO;
    [self refreshTitle];
}

- (void)togglePause:(id)sender {
    (void)sender;
    if (!_source || !_source->live()) return;
    if (_paused) {
        _paused = NO;
        [self refreshTitle];
        return;
    }
    [self showPauseMenu];
}

- (void)restartLevel:(id)sender {
    (void)sender;
    if (!_source) return;
    std::string description;
    if (!_source->restart(description)) {
        NSAlert* a = [[NSAlert alloc] init];
        a.messageText = @"Could not restart level";
        a.informativeText = [NSString stringWithUTF8String:description.c_str()];
        [a runModal];
        return;
    }
    _paused = NO;
    NSString* detail = [NSString stringWithUTF8String:description.c_str()];
    _baseTitle = [NSString stringWithFormat:@"Deimos Rising Remastered - %@", detail];
    [self refreshTitle];
    if (_host) (void)present_host(*_host, _source->frame());
}

- (void)showControls:(id)sender {
    (void)sender;
    BOOL wasPaused = _paused;
    _paused = YES;
    if (_source) _source->clear_control();
    [self refreshTitle];

    NSAlert* a = [[NSAlert alloc] init];
    a.messageText = @"Deimos Rising Controls";
    a.informativeText =
        @"ORIGINAL 1.0.6 PLAYER-1 DEFAULTS\n"
         @"Arrow Keys  Move\n"
         @"Option      Fire Air Weapon\n"
         @"Command     Fire Ground Weapon\n"
         @"Space       Select / Cycle Air Weapon\n\n"
         @"MODERN HOST ALIASES\n"
         @"WASD        Move\n"
         @"Z           Fire / hold to charge Air Weapon\n"
         @"X or Shift  Fire Ground Weapon / Plasma Bomb\n"
         @"C or Tab    Select / cycle Air Weapon\n"
         @"Escape      Pause / Resume\n\n"
         @"Ground weapon aiming uses the cyan targeting reticle ahead of the ship; it changes frame when a valid ground target is locked.";
    [a addButtonWithTitle:@"Resume"];
    [a runModal];
    _paused = wasPaused;
    [self refreshTitle];
}

- (void)showPreferences:(id)sender {
    (void)sender;
    BOOL wasPaused = _paused;
    _paused = YES;
    if (_source) _source->clear_control();
    [self refreshTitle];

    NSAlert* a = [[NSAlert alloc] init];
    a.messageText = @"Deimos Rising Preferences";
    a.informativeText =
        @"The original 1.0.6 Preferences dialog exposed Full Screen, Interlacing, Bypass System Volume, Music Volume, Sound Volume, ESC Key Delay, Set Controls, and Set Gamepad Controls.\n\n"
         @"This remaster currently uses the original 640×480 game presentation with nearest-neighbor scaling. Full Screen is available from the View menu. Audio/gamepad preference controls will become active as their recovered runtimes are connected.";
    [a addButtonWithTitle:@"OK"];
    [a addButtonWithTitle:@"Controls…"];
    NSModalResponse r = [a runModal];
    if (r == NSAlertSecondButtonReturn) [self showControls:nil];
    _paused = wasPaused;
    [self refreshTitle];
}

- (void)applicationDidFinishLaunching:(NSNotification*)notification {
    (void)notification;
    _paused = NO;
    [self installApplicationMenus];
    _host = std::make_unique<deimos::AppleMetalHostView>();
    _source = std::make_unique<SmokeFrameSource>();
    std::string frameDescription;
    if (!_source->initialize(frameDescription)) {
        NSAlert* alert = [[NSAlert alloc] init];
        alert.messageText = @"Deimos Rising playable host could not start";
        alert.informativeText = [NSString stringWithUTF8String:frameDescription.c_str()];
        [alert addButtonWithTitle:@"Quit"];
        [alert runModal];
        [NSApp terminate:nil];
        return;
    }

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
    _window = [[DeimosGameWindow alloc]
        initWithContentRect:frameRect
                  styleMask:(NSWindowStyleMaskTitled |
                             NSWindowStyleMaskClosable |
                             NSWindowStyleMaskMiniaturizable |
                             NSWindowStyleMaskResizable)
                    backing:NSBackingStoreBuffered
                      defer:NO];
    NSString* detail = [NSString stringWithUTF8String:frameDescription.c_str()];
    _baseTitle = [NSString stringWithFormat:@"Deimos Rising Remastered - %@", detail];
    _window.title = _baseTitle;
    _window.delegate = self;

    NSView* metalView = (__bridge NSView*)_host->native_view_handle();
    metalView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    _window.contentView = metalView;
    [_window center];
    [_window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];

    // Use the key window responder path directly. The earlier local NSEvent
    // monitor could leave the app apparently playable while semantic weapon
    // input never reached the source. Keep physical-key mappings explicit and
    // preserve the original Player-1 modifier/Space defaults while adding modern aliases.
    SmokeFrameSource* source = _source.get();
    DeimosSmokeAppDelegate* delegate = self;
    DeimosGameWindow* gameWindow = (DeimosGameWindow*)_window;
    gameWindow.deimosKeyHandler = ^(NSEvent* event, BOOL pressedValue) {
        const bool pressed = pressedValue == YES;
        switch (event.keyCode) {
            case 123: // left arrow
            case 0:   // A
                source->set_control_direction(SmokeFrameSource::ControlDirection::Left, pressed);
                break;
            case 124: // right arrow
            case 2:   // D
                source->set_control_direction(SmokeFrameSource::ControlDirection::Right, pressed);
                break;
            case 126: // up arrow
            case 13:  // W
                source->set_control_direction(SmokeFrameSource::ControlDirection::Up, pressed);
                break;
            case 125: // down arrow
            case 1:   // S
                source->set_control_direction(SmokeFrameSource::ControlDirection::Down, pressed);
                break;
            case 6:   // Z: modern primary-air alias
            case 58:  // Left Option: original Player-1 Fire Air Weapon
            case 61:  // Right Option
                source->set_weapon_action(SmokeFrameSource::WeaponAction::FireAir, pressed);
                break;
            case 7:   // X: modern ground-weapon alias
            case 56:  // Left Shift: secondary/ground alias
            case 60:  // Right Shift
            case 55:  // Left Command: original Player-1 Fire Ground Weapon
            case 54:  // Right Command
                source->set_weapon_action(SmokeFrameSource::WeaponAction::FireGround, pressed);
                break;
            case 49:  // Space: original Player-1 Select Special Weapon
            case 48:  // Tab: modern select alias
            case 8:   // C
                source->set_weapon_action(SmokeFrameSource::WeaponAction::SwitchAir, pressed);
                break;
            case 53:  // Escape: pause/resume
                if (pressed) [delegate togglePause:nil];
                break;
            default:
                break;
        }
    };
    [_window makeFirstResponder:_window];

    (void)_host->sync_drawable_geometry(&error);
    (void)present_host(*_host, _source->frame());
    [self showLaunchMenu];

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
    if (!_host || !_source || !_source->live() || _paused) return;
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

- (void)windowDidResignKey:(NSNotification*)notification {
    (void)notification;
    if (_source) _source->clear_control();
}

- (void)windowWillClose:(NSNotification*)notification {
    (void)notification;
    [_timer invalidate];
    _timer = nil;
    if ([_window isKindOfClass:[DeimosGameWindow class]]) {
        ((DeimosGameWindow*)_window).deimosKeyHandler = nil;
    }
    if (_source) _source->clear_control();
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
