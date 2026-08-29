#include "deimos/frame_timing_runtime.hpp"

#include <algorithm>
#include <cmath>

namespace deimos {

std::optional<LegacyFrameTimingConfig> compile_legacy_frame_timing_config(
    const NamedTable<float>& game_floats,
    std::string* error) {
    const auto it = std::find_if(game_floats.begin(), game_floats.end(), [](const auto& entry) {
        return entry.first == "FPS_MaxRate";
    });
    if (it == game_floats.end()) {
        if (error) *error = "Game[gafl] is missing FPS_MaxRate";
        return std::nullopt;
    }
    if (!std::isfinite(it->second) || it->second <= 0.0f) {
        if (error) *error = "Game[gafl] FPS_MaxRate must be finite and positive";
        return std::nullopt;
    }
    return LegacyFrameTimingConfig{it->second};
}

} // namespace deimos
