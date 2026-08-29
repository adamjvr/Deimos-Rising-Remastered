#pragma once

#include "deimos/data_tables.hpp"
#include "deimos/entity_runtime.hpp"
#include "deimos/render_backend.hpp"

#include <array>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace deimos {


// PPC 0x100A0/0x100B0 horizontal renderer-view controller. The offset is
// constrained to [-32,31]. A requested step moves by exactly one pixel; the
// direction latch is -1/+1 only when movement remains inside the bounds and
// is left at zero when a requested step saturates at either hard limit.
struct LegacyHorizontalViewRuntime {
    int offset = 0;
    int direction = 0;
};

void step_legacy_horizontal_view(LegacyHorizontalViewRuntime& view, bool positive_direction);

// Game[gafl] indices 54..56 consumed by PPC 0xFA90/0xFBC0/0x10120/0x10220.
// The +32 horizontal source bias and 64-pixel ahead-of-camera row margin are
// executable literals rather than data-table values.
struct LegacyTerrainSurfaceConfig {
    int visible_width = 0;
    int visible_height = 0;
    int display_depth = 0;
    int horizontal_source_bias = 32;
    int row_activation_margin = 64;
};

[[nodiscard]] std::optional<LegacyTerrainSurfaceConfig> compile_legacy_terrain_surface_config(
    const NamedTable<float>& game_floats,
    std::string* error = nullptr);

// Camera/source-rectangle state owned by the classic terrain module. The
// persistent terrain pixels themselves live in the caller-supplied full-size
// LegacyRasterSurface; this state survives across frames while layer 0/1
// requests mutate that same surface.
struct LegacyTerrainSurfaceRuntime {
    LegacyTerrainSurfaceConfig config{};
    LegacyRasterRect full_bounds{};
    LegacyRasterRect source_view{};
    int requested_vertical_delta = 1;
    int applied_vertical_delta = 0;
    int vertical_progress = 0;
    bool reached_end = false;
    bool row_updates_suppressed = false;
};

// Clean initialization counterpart of 0xFA90 after 0xFBC0 has loaded/copied
// the full background into its persistent 16-bit surface. The initial view is
// the bottom-most visible-height crop with the fixed +32 source-X bias, and
// vertical_progress starts at visible_height+1 (481 in canonical 1.0.6).
[[nodiscard]] bool initialize_legacy_terrain_surface_runtime(
    LegacyTerrainSurfaceRuntime& runtime,
    const LegacyRasterSurface& persistent_terrain,
    const LegacyTerrainSurfaceConfig& config,
    std::string* error = nullptr);

// PPC 0xFA10 pre-activates every world row from source bottom through 64 pixels
// above source top (inclusive): canonical 480-high gameplay therefore emits
// exactly 545 callbacks. Suppressed mode performs no callbacks.
using LegacyTerrainRowUpdate = std::function<void(int world_y)>;
void prime_legacy_terrain_rows(
    const LegacyTerrainSurfaceRuntime& runtime,
    const LegacyTerrainRowUpdate& row_update);

// PPC 0x10220 updates only the source rectangle and scroll accounting; it does
// not copy bitmap strips. requested_vertical_delta is subtracted from top and
// bottom, progress is clamped to full_bounds, and applied_vertical_delta is
// oldTop-finalTop after source-surface clamping.
void step_legacy_vertical_terrain_view(
    LegacyTerrainSurfaceRuntime& runtime,
    const LegacyRasterSurface& persistent_terrain);

// PPC 0x10000 wraps 0x10220, performs the original end latch, and (unless
// suppressed) activates the row exactly 64 pixels above the new source top.
// Returns the legacy "reached end" boolean for this tick.
[[nodiscard]] bool tick_legacy_terrain_scroll(
    LegacyTerrainSurfaceRuntime& runtime,
    const LegacyRasterSurface& persistent_terrain,
    const LegacyTerrainRowUpdate& row_update = {});

// PPC 0x10120 is a full viewport CopyBits-style copy, not an incremental strip
// update. It clones source_view, replaces X with max(horizontal.offset+32,0),
// sets right=left+VisibleGameWidth, and copies that entire rectangle to a
// {0,0,VisibleGameHeight,VisibleGameWidth} destination. This is the core pixel
// persistence boundary: terrain stamps remain in persistent_terrain and are
// re-exposed whenever the moving source view reaches them.
[[nodiscard]] bool copy_legacy_terrain_viewport(
    const LegacyTerrainSurfaceRuntime& runtime,
    const LegacyHorizontalViewRuntime& horizontal,
    const LegacyRasterSurface& persistent_terrain,
    LegacyRasterSurface& visible_surface,
    std::string* error = nullptr);

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
