#include "deimos/frame_timing_runtime.hpp"

#include <cassert>
#include <cmath>
#include <limits>
#include <string>

int main() {
    using namespace deimos;

    std::string error;
    const NamedTable<float> canonical_shape{
        {"WepSelector_FadeOutRate", 2.0f},
        {"Particle_Gravity", 0.96f},
        {"FPS_MaxRate", 30.0f},
    };
    const auto timing = compile_legacy_frame_timing_config(canonical_shape, &error);
    assert(timing);
    assert(timing->fps_max_rate == 30.0f);
    assert(std::abs(timing->seconds_per_frame() - (1.0 / 30.0)) < 1e-12);

    error.clear();
    assert(!compile_legacy_frame_timing_config({{"Other", 30.0f}}, &error));
    assert(error.find("missing FPS_MaxRate") != std::string::npos);

    error.clear();
    assert(!compile_legacy_frame_timing_config({{"FPS_MaxRate", 0.0f}}, &error));
    assert(error.find("finite and positive") != std::string::npos);

    error.clear();
    assert(!compile_legacy_frame_timing_config(
        {{"FPS_MaxRate", std::numeric_limits<float>::infinity()}}, &error));
    return 0;
}
