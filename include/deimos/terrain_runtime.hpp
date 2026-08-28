#pragma once

#include "deimos/data_tables.hpp"
#include "deimos/entity_runtime.hpp"

#include <array>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace deimos {

// Persistent rectangle list owned by the classic background/terrain module.
// PPC 0x2A6D0 appends one QuickDraw-style Rect, 0x2A770 shifts every rect by
// the vertical terrain-scroll delta, and 0x2A830 performs inclusive overlap.
class LegacyGroundObstacleRects {
public:
    void add(RectI rect);
    void shift_vertical(int delta);
    [[nodiscard]] bool overlaps(RectI rect) const;
    void clear();

    [[nodiscard]] std::size_t size() const { return rects_.size(); }
    [[nodiscard]] const std::vector<RectI>& rects() const { return rects_; }

private:
    std::vector<RectI> rects_;
};

// Semantic conversion of the PPC 0x12A00 live-member rectangle. The original
// memory layout is QuickDraw {top,left,bottom,right}; RectI stores the same
// values by semantic name.
[[nodiscard]] RectI legacy_entity_world_rect(const EntityRuntime& entity);

// Main-tick live +0x19 is a constructor-cached air-domain flag derived from
// UnitDef +0x08 ('air ' vs 'grnd'). Only ground-domain members then test
// collidesWithGroundObstacles_BOOL (+0x128) and query the persistent Rect list.
[[nodiscard]] bool legacy_collides_with_ground_obstacle(
    const EntityRuntime& entity,
    const LegacyGroundObstacleRects& obstacles);

// Consequence of a successful 0x2A830 query in the main member tick around
// 0x34538. The shared two-float source copied by the PPC is the canonical
// {0,0} vector, also used by constructor/motion code: the hit zeros velocity
// and latches live +0x13C stationary. It does NOT restore/rollback position.
// destructDrawToTerrain_BOOL additionally appends the current entity Rect to
// the same persistent obstacle list. Returns false when no hit occurred.
[[nodiscard]] bool apply_legacy_ground_obstacle_stop(
    EntityRuntime& entity,
    LegacyGroundObstacleRects& obstacles);

// Fixed Objects[gaob] positional resource contract consumed by PPC 0x16880.
// Slots 6..9 are label-verified water-impact effects.
struct LegacyWaterImpactConfig {
    FourCC tiny{};
    FourCC small{};
    FourCC medium{};
    FourCC large{};
};

[[nodiscard]] std::optional<LegacyWaterImpactConfig> compile_legacy_water_impact_config(
    const NamedTable<FourCC>& game_objects,
    std::string* error = nullptr);

// The executable samples the Media Mask after transforming the live-member
// point to (trunc(x)+32, trunc(y)+world_y_origin). 0xFEE0 returns true only
// when the sampled 16-bit mask cell equals 31. The surrounding 0x16880 code
// identifies that path as water by selecting Objects[gaob] Water impact IDs.
using LegacyWaterMaskProbe = std::function<bool(int world_x, int world_y)>;

struct LegacyRemovalMediaResult {
    bool water_hit = false;
    bool allow_requested_spawn = true;
    int sample_x = 0;
    int sample_y = 0;
    std::optional<FourCC> replacement_spawn;
    int rng_draws = 0;
};

// Exact clean mirror of PPC 0x16880's decision core. Air units and ground
// units with doDeathSpawnOnAnyMedia_BOOL bypass the mask and allow the caller's
// requested death/deletion spawn. A water hit suppresses that requested spawn;
// mediaImpactSize_ID may cause 0x16880 itself to emit a water-impact replacement.
[[nodiscard]] LegacyRemovalMediaResult resolve_legacy_removal_media(
    const EntityRuntime& entity,
    int world_y_origin,
    const LegacyWaterImpactConfig& config,
    const LegacyWaterMaskProbe& water_probe,
    LegacyRandom& random);

} // namespace deimos
