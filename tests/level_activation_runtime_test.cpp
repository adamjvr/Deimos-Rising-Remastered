#include "deimos/level_activation_runtime.hpp"
#include "deimos/terrain_runtime.hpp"

#include <cassert>
#include <iostream>
#include <vector>

using namespace deimos;

int main() {
    LevelDefinition level;
    level.objects = {
        {{{'a','0','0','1'}}, {{'a','i','r',' '}}, 10, 900, 0, false, false},
        {{{'b','0','0','1'}}, {{'a','i','r',' '}}, 20, 700, 0, false, false},
        {{{'c','0','0','1'}}, {{'g','r','n','d'}}, 30, 900, 0, false, false},
        {{{'d','0','0','1'}}, {{'g','r','n','d'}}, 40, 455, 0, false, false},
    };

    LevelPlacementActivationRuntime activation;
    activation.reset(level.objects.size());

    const auto row900 = activation.activate_row(level, 900);
    assert((row900 == std::vector<std::size_t>{0, 2}));
    assert(activation.activated_count() == 2);
    assert(activation.pending_count() == 2);
    assert(activation.activated(0));
    assert(!activation.activated(1));
    assert(activation.activated(2));

    // A world row is one-shot even if the terrain callback is repeated.
    assert(activation.activate_row(level, 900).empty());

    // Couple the placement runtime to the already-frozen terrain row sequence.
    LegacyTerrainSurfaceConfig config;
    config.visible_width = 416;
    config.visible_height = 480;
    config.display_depth = 16;
    config.horizontal_source_bias = 32;
    config.row_activation_margin = 64;
    LegacyRasterSurface terrain(480, 1000, 0);
    LegacyTerrainSurfaceRuntime runtime;
    std::string error;
    assert(initialize_legacy_terrain_surface_runtime(runtime, terrain, config, &error));

    activation.reset(level.objects.size());
    std::vector<std::size_t> primed;
    prime_legacy_terrain_rows(runtime, [&](int y) {
        const auto released = activation.activate_row(level, y);
        primed.insert(primed.end(), released.begin(), released.end());
    });
    // Initial prime is world rows 1000..456 inclusive: both y=900 entries and
    // y=700 activate, while y=455 remains exactly one row beyond the margin.
    assert((primed == std::vector<std::size_t>{0, 2, 1}));
    assert(activation.activated_count() == 3);

    runtime.requested_vertical_delta = 1;
    bool released455 = false;
    for (int i = 0; i < 65 && !released455; ++i) {
        (void)tick_legacy_terrain_scroll(runtime, terrain, [&](int y) {
            const auto released = activation.activate_row(level, y);
            released455 = !released.empty() && released.front() == 3;
        });
    }
    assert(released455);
    assert(activation.activated_count() == 4);
    assert(activation.pending_count() == 0);

    std::cout << "level_activation_runtime_test: PASS\n";
    return 0;
}
