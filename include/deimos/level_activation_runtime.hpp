#pragma once

#include "deimos/level.hpp"

#include <cstddef>
#include <vector>

namespace deimos {

// Portable boundary for the world-row activation callback reached from PPC
// 0xFA10/0x10000 through world routine 0x33090. Terrain code supplies exact
// world-Y rows; this runtime releases each level placement once when its yLoc
// matches the activated row, preserving source-document order for placements
// that share a row.
//
// The terrain row range/timing is instruction-closed. The yLoc equality bridge
// is the minimal level-format interpretation consistent with the recovered
// row-callback contract and remains explicitly isolated for PPC Lab validation.
class LevelPlacementActivationRuntime {
public:
    void reset(std::size_t placement_count);

    [[nodiscard]] std::vector<std::size_t> activate_row(
        const LevelDefinition& level,
        int world_y);

    [[nodiscard]] bool activated(std::size_t placement_index) const noexcept;
    [[nodiscard]] std::size_t activated_count() const noexcept { return activated_count_; }
    [[nodiscard]] std::size_t pending_count() const noexcept {
        return activated_.size() - activated_count_;
    }

private:
    std::vector<bool> activated_{};
    std::size_t activated_count_ = 0;
};

} // namespace deimos
