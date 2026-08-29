#pragma once

#include "deimos/data_tables.hpp"

#include <optional>
#include <string>

namespace deimos {

// Named Game[gafl] timing value used by the original data set. The field name
// is stable but its table position is not treated as an ABI contract here.
// This clean structure intentionally models only the source-data fact proven so
// far; the exact classic Mac timer/sleep implementation remains a separate
// reverse-engineering boundary.
struct LegacyFrameTimingConfig {
    float fps_max_rate = 30.0f;

    [[nodiscard]] double seconds_per_frame() const noexcept {
        return fps_max_rate > 0.0f ? 1.0 / static_cast<double>(fps_max_rate) : 0.0;
    }
};

[[nodiscard]] std::optional<LegacyFrameTimingConfig> compile_legacy_frame_timing_config(
    const NamedTable<float>& game_floats,
    std::string* error = nullptr);

} // namespace deimos
