#include "deimos/level_activation_runtime.hpp"

namespace deimos {

void LevelPlacementActivationRuntime::reset(std::size_t placement_count) {
    activated_.assign(placement_count, false);
    activated_count_ = 0;
}

std::vector<std::size_t> LevelPlacementActivationRuntime::activate_row(
    const LevelDefinition& level,
    int world_y) {
    if (activated_.size() != level.objects.size()) {
        reset(level.objects.size());
    }

    std::vector<std::size_t> released;
    for (std::size_t i = 0; i < level.objects.size(); ++i) {
        if (activated_[i] || level.objects[i].y != world_y) continue;
        activated_[i] = true;
        ++activated_count_;
        released.push_back(i);
    }
    return released;
}

bool LevelPlacementActivationRuntime::activated(std::size_t placement_index) const noexcept {
    return placement_index < activated_.size() && activated_[placement_index];
}

} // namespace deimos
