#include "deimos/terrain_runtime.hpp"

#include "deimos/collision_runtime.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>

namespace deimos {

void step_legacy_horizontal_view(LegacyHorizontalViewRuntime& view, bool positive_direction) {
    // 0x100B0 clears the direction latch first, then applies +/-1. Saturating
    // at a hard edge returns before re-latching direction.
    view.direction = 0;
    const int delta = positive_direction ? 1 : -1;
    view.offset += delta;
    if (delta < 0) {
        if (view.offset < -32) {
            view.offset = -32;
            return;
        }
        view.direction = -1;
        return;
    }
    if (view.offset > 31) {
        view.offset = 31;
        return;
    }
    view.direction = 1;
}


std::optional<LegacyTerrainSurfaceConfig> compile_legacy_terrain_surface_config(
    const NamedTable<float>& game_floats,
    std::string* error) {
    constexpr std::size_t kFirst = 54;
    constexpr std::array<std::string_view, 3> labels = {
        "VisibleGameWidth", "VisibleGameHeight", "ReqDisplayDepth"
    };
    if (game_floats.size() < kFirst + labels.size()) {
        if (error) *error = "Game[gafl] is shorter than the 1.0.6 terrain-surface positional contract";
        return std::nullopt;
    }
    for (std::size_t i = 0; i < labels.size(); ++i) {
        if (game_floats[kFirst + i].first != labels[i]) {
            if (error) {
                *error = "unexpected Game[gafl] terrain-surface label at index " +
                    std::to_string(kFirst + i);
            }
            return std::nullopt;
        }
    }

    const auto trunc_i = [](float value) { return static_cast<int>(std::trunc(value)); };
    LegacyTerrainSurfaceConfig out;
    out.visible_width = trunc_i(game_floats[54].second);
    out.visible_height = trunc_i(game_floats[55].second);
    out.display_depth = trunc_i(game_floats[56].second);
    if (out.visible_width <= 0 || out.visible_height <= 0) {
        if (error) *error = "terrain visible dimensions must be positive";
        return std::nullopt;
    }
    if (out.display_depth != 16) {
        if (error) *error = "clean terrain compositor currently requires the legacy 16-bit display depth";
        return std::nullopt;
    }
    return out;
}

bool initialize_legacy_terrain_surface_runtime(
    LegacyTerrainSurfaceRuntime& runtime,
    const LegacyRasterSurface& persistent_terrain,
    const LegacyTerrainSurfaceConfig& config,
    std::string* error) {
    if (!persistent_terrain.valid()) {
        if (error) *error = "persistent terrain surface is invalid";
        return false;
    }
    if (config.visible_width <= 0 || config.visible_height <= 0 || config.display_depth != 16) {
        if (error) *error = "invalid legacy terrain-surface configuration";
        return false;
    }
    if (persistent_terrain.height < config.visible_height) {
        if (error) *error = "persistent terrain is shorter than VisibleGameHeight";
        return false;
    }
    if (persistent_terrain.width < config.horizontal_source_bias + config.visible_width) {
        if (error) *error = "persistent terrain is narrower than the initial +32 gameplay source view";
        return false;
    }

    runtime.config = config;
    runtime.full_bounds = persistent_terrain.bounds();
    runtime.source_view.top = runtime.full_bounds.bottom - config.visible_height;
    runtime.source_view.left = config.horizontal_source_bias;
    if (runtime.source_view.left < 0) runtime.source_view.left = 0;
    runtime.source_view.bottom = runtime.source_view.top + config.visible_height;
    runtime.source_view.right = runtime.source_view.left + config.visible_width;
    runtime.requested_vertical_delta = 1;
    runtime.applied_vertical_delta = 0;
    runtime.vertical_progress = config.visible_height + 1;
    runtime.reached_end = false;
    runtime.row_updates_suppressed = false;
    return true;
}

void prime_legacy_terrain_rows(
    const LegacyTerrainSurfaceRuntime& runtime,
    const LegacyTerrainRowUpdate& row_update) {
    if (runtime.row_updates_suppressed || !row_update) return;

    // 0xFA48 computes top-65, then loops i < bottom-(top-65), invoking
    // 0x33090(bottom-i). This includes both bottom and top-64.
    const int count = runtime.source_view.bottom -
        (runtime.source_view.top - (runtime.config.row_activation_margin + 1));
    for (int i = 0; i < count; ++i) {
        row_update(runtime.source_view.bottom - i);
    }
}

void step_legacy_vertical_terrain_view(
    LegacyTerrainSurfaceRuntime& runtime,
    const LegacyRasterSurface& persistent_terrain) {
    const int delta = runtime.requested_vertical_delta;
    if (delta == 0) {
        runtime.applied_vertical_delta = 0;
        return;
    }

    const int old_top = runtime.source_view.top;
    runtime.source_view.top -= delta;
    runtime.vertical_progress += delta;
    runtime.vertical_progress = std::clamp(
        runtime.vertical_progress, 0, runtime.full_bounds.bottom);
    runtime.source_view.bottom -= delta;

    // 0x102A4 uses <= 0, not < 0.
    if (runtime.source_view.top <= 0) {
        runtime.source_view.top = 0;
        runtime.source_view.bottom = runtime.config.visible_height;
    }

    // 0x102D4 fetches the persistent background surface bounds dynamically.
    // It only tests/clamps the lower edge here; the top-edge case above is
    // independent and intentionally preserves the original ordering.
    const auto terrain_bounds = persistent_terrain.bounds();
    if (runtime.source_view.bottom > terrain_bounds.bottom) {
        runtime.source_view.bottom = terrain_bounds.bottom;
        runtime.source_view.top = terrain_bounds.bottom - runtime.config.visible_height;
    }

    runtime.applied_vertical_delta = old_top - runtime.source_view.top;
}

bool tick_legacy_terrain_scroll(
    LegacyTerrainSurfaceRuntime& runtime,
    const LegacyRasterSurface& persistent_terrain,
    const LegacyTerrainRowUpdate& row_update) {
    step_legacy_vertical_terrain_view(runtime, persistent_terrain);

    if (runtime.requested_vertical_delta == 0) {
        return runtime.reached_end;
    }

    if (runtime.vertical_progress >= runtime.full_bounds.bottom) {
        runtime.vertical_progress = runtime.full_bounds.bottom;
        runtime.reached_end = true;
        runtime.requested_vertical_delta = 0;
        return true;
    }

    if (!runtime.row_updates_suppressed && row_update) {
        row_update(runtime.source_view.top - runtime.config.row_activation_margin);
    }
    return false;
}

bool copy_legacy_terrain_viewport(
    const LegacyTerrainSurfaceRuntime& runtime,
    const LegacyHorizontalViewRuntime& horizontal,
    const LegacyRasterSurface& persistent_terrain,
    LegacyRasterSurface& visible_surface,
    std::string* error) {
    if (!persistent_terrain.valid() || !visible_surface.valid()) {
        if (error) *error = "invalid terrain or visible raster surface";
        return false;
    }

    auto source = runtime.source_view;
    source.left = horizontal.offset + runtime.config.horizontal_source_bias;
    if (source.left < 0) source.left = 0;
    source.right = source.left + runtime.config.visible_width;
    const LegacyRasterRect destination{
        0, 0, runtime.config.visible_height, runtime.config.visible_width};

    if (source.top < 0 || source.left < 0 || source.bottom > persistent_terrain.height ||
        source.right > persistent_terrain.width || source.empty()) {
        if (error) *error = "legacy terrain source viewport falls outside the persistent surface";
        return false;
    }
    if (destination.bottom > visible_surface.height || destination.right > visible_surface.width) {
        if (error) *error = "visible surface is smaller than the legacy gameplay viewport";
        return false;
    }

    for (int row = 0; row < runtime.config.visible_height; ++row) {
        const auto src_index = static_cast<std::size_t>(source.top + row) *
            static_cast<std::size_t>(persistent_terrain.width) + static_cast<std::size_t>(source.left);
        const auto dst_index = static_cast<std::size_t>(row) *
            static_cast<std::size_t>(visible_surface.width);
        std::copy_n(
            persistent_terrain.pixels.begin() + static_cast<std::ptrdiff_t>(src_index),
            runtime.config.visible_width,
            visible_surface.pixels.begin() + static_cast<std::ptrdiff_t>(dst_index));
    }
    return true;
}

namespace {

constexpr FourCC fourcc(char a, char b, char c, char d) {
    return FourCC{{a, b, c, d}};
}

bool is_ground(const EntityRuntime& entity) {
    return entity.behavior.collision_domain == fourcc('g', 'r', 'n', 'd');
}

bool empty_or_none(FourCC id) {
    return id == FourCC{} || id.str() == "none" || id.str() == "NULL";
}

} // namespace

void LegacyGroundObstacleRects::add(RectI rect) {
    // 0x2A6D0 appends without de-duplication or merging.
    rects_.push_back(rect);
}

void LegacyGroundObstacleRects::shift_vertical(int delta) {
    // Raw PPC Rect offsets 0/+8 are top/bottom, respectively.
    for (auto& rect : rects_) {
        rect.top += delta;
        rect.bottom += delta;
    }
}

bool LegacyGroundObstacleRects::overlaps(RectI rect) const {
    // PPC 0x2A8A4..0x2A8E4 rejects only strict separation, making touching
    // edges count as overlap.
    for (const auto& obstacle : rects_) {
        if (rect.bottom < obstacle.top) continue;
        if (rect.top > obstacle.bottom) continue;
        if (rect.right < obstacle.left) continue;
        if (rect.left > obstacle.right) continue;
        return true;
    }
    return false;
}

void LegacyGroundObstacleRects::clear() {
    rects_.clear();
}

RectI legacy_entity_world_rect(const EntityRuntime& entity) {
    const auto bounds = legacy_collision_bounds(entity);
    return {bounds.min_x, bounds.min_y, bounds.max_x, bounds.max_y};
}

bool legacy_collides_with_ground_obstacle(
    const EntityRuntime& entity,
    const LegacyGroundObstacleRects& obstacles) {
    // Main tick 0x344F8 first tests live +0x19. Constructor 0x35F88..0x35FA0
    // derives that byte as (UnitDef +0x08 == 'air '), so only ground-domain
    // members proceed to collidesWithGroundObstacles_BOOL at UnitDef +0x128.
    if (!is_ground(entity)) return false;
    if (!entity.behavior.collides_with_ground_obstacles) return false;
    return obstacles.overlaps(legacy_entity_world_rect(entity));
}

bool apply_legacy_ground_obstacle_stop(
    EntityRuntime& entity,
    LegacyGroundObstacleRects& obstacles) {
    // 0x34504 first rejects members already carrying the stationary latch.
    if (entity.stationary) return false;
    if (!legacy_collides_with_ground_obstacle(entity, obstacles)) return false;

    // 0x34544..0x34558 copies the engine's canonical zero vector to live
    // +0x10/+0x14, then sets +0x13C. Earlier notes incorrectly described
    // these writes as a position rollback; +0x10/+0x14 are velocity.
    entity.velocity_x = 0.0f;
    entity.velocity_y = 0.0f;
    entity.stationary = true;

    // 0x34560..0x34578: draw-to-terrain entities feed their current Rect back
    // into the persistent ground-obstacle list after being stopped.
    if (entity.behavior.destruction_draw_to_terrain) {
        obstacles.add(legacy_entity_world_rect(entity));
    }
    return true;
}

std::optional<LegacyWaterImpactConfig> compile_legacy_water_impact_config(
    const NamedTable<FourCC>& game_objects,
    std::string* error) {
    constexpr std::size_t kFirst = 6;
    constexpr std::array<std::string_view, 4> names = {
        "MediaImpact_Water_Tiny",
        "MediaImpact_Water_Small",
        "MediaImpact_Water_Medium",
        "MediaImpact_Water_Large"
    };
    if (game_objects.size() < kFirst + names.size()) {
        if (error) *error = "Objects[gaob] is shorter than the 1.0.6 water-impact positional contract";
        return std::nullopt;
    }
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (game_objects[kFirst + i].first != names[i]) {
            if (error) {
                *error = "unexpected Objects[gaob] water-impact label at index " +
                    std::to_string(kFirst + i);
            }
            return std::nullopt;
        }
    }
    return LegacyWaterImpactConfig{
        game_objects[6].second,
        game_objects[7].second,
        game_objects[8].second,
        game_objects[9].second
    };
}

std::optional<LegacyMediaMaskGeometry> compile_legacy_media_mask_geometry(
    const LegacyRasterSurface& media_mask,
    RectI world_bounds,
    std::string* error) {
    if (!media_mask.valid()) {
        if (error) *error = "Media Mask surface is invalid";
        return std::nullopt;
    }
    const int world_width = world_bounds.right - world_bounds.left;
    const int world_height = world_bounds.bottom - world_bounds.top;
    if (world_width <= 0 || world_height <= 0) {
        if (error) *error = "Media Mask world rectangle must have positive dimensions";
        return std::nullopt;
    }
    if (world_width % media_mask.width != 0 || world_height % media_mask.height != 0) {
        if (error) *error = "Media Mask dimensions do not divide the level background rectangle exactly";
        return std::nullopt;
    }

    LegacyMediaMaskGeometry geometry;
    geometry.world_bounds = world_bounds;
    geometry.world_pixels_per_cell_x = world_width / media_mask.width;
    geometry.world_pixels_per_cell_y = world_height / media_mask.height;
    if (geometry.world_pixels_per_cell_x <= 0 || geometry.world_pixels_per_cell_y <= 0) {
        if (error) *error = "Media Mask world-to-cell scale must be positive";
        return std::nullopt;
    }
    return geometry;
}

bool legacy_media_mask_is_water(
    const LegacyRasterSurface& media_mask,
    const LegacyMediaMaskGeometry& geometry,
    int world_x,
    int world_y) {
    if (!media_mask.valid() || geometry.world_pixels_per_cell_x <= 0 ||
        geometry.world_pixels_per_cell_y <= 0) return false;
    if (world_x < geometry.world_bounds.left || world_y < geometry.world_bounds.top ||
        world_x >= geometry.world_bounds.right || world_y >= geometry.world_bounds.bottom) {
        return false;
    }

    const int mask_x = (world_x - geometry.world_bounds.left) /
        geometry.world_pixels_per_cell_x;
    const int mask_y = (world_y - geometry.world_bounds.top) /
        geometry.world_pixels_per_cell_y;
    if (mask_x < 0 || mask_y < 0 || mask_x >= media_mask.width || mask_y >= media_mask.height) {
        return false;
    }
    const auto index = static_cast<std::size_t>(mask_y) *
        static_cast<std::size_t>(media_mask.width) + static_cast<std::size_t>(mask_x);
    return media_mask.pixels[index] == 31u;
}

LegacyRemovalMediaResult resolve_legacy_removal_media(
    const EntityRuntime& entity,
    int world_y_origin,
    const LegacyWaterImpactConfig& config,
    const LegacyWaterMaskProbe& water_probe,
    LegacyRandom& random) {
    LegacyRemovalMediaResult result;

    // 0x1689C/0x168A8: non-ground units and the explicit any-media override
    // return true before sampling the Media Mask.
    if (!is_ground(entity) || entity.behavior.death_spawn_on_any_media) {
        return result;
    }

    // 0x168C4..0x168F8 use fctiwz independently before the fixed offsets.
    result.sample_x = static_cast<int>(std::trunc(entity.x)) + 32;
    result.sample_y = static_cast<int>(std::trunc(entity.y)) + world_y_origin;

    // A clean headless caller may not have a loaded Media Mask. Preserve the
    // previous ordinary-spawn behavior in that case instead of guessing water.
    if (!water_probe || !water_probe(result.sample_x, result.sample_y)) {
        return result;
    }

    result.water_hit = true;
    result.allow_requested_spawn = false;

    const FourCC size = entity.behavior.media_impact_size;
    if (empty_or_none(size)) return result;

    if (size == fourcc('t', 'i', 'n', 'y')) {
        result.replacement_spawn = config.tiny;
    } else if (size == fourcc('s', 'm', 'a', 'l')) {
        result.replacement_spawn = config.small;
    } else if (size == fourcc('m', 'e', 'd', ' ')) {
        result.replacement_spawn = config.medium;
    } else if (size == fourcc('l', 'a', 'r', 'g')) {
        result.replacement_spawn = config.large;
    } else if (size == fourcc('s', 'm', 'r', 'a')) {
        // 0x16A1C: range 0..1, but the switch order is intentionally reversed:
        // roll 0 -> Small, roll 1 -> Tiny.
        const int roll = choose_inclusive_integer(0, 1, random);
        ++result.rng_draws;
        result.replacement_spawn = roll == 0 ? config.small : config.tiny;
    } else if (size == fourcc('m', 'e', 'r', 'a')) {
        const int roll = choose_inclusive_integer(0, 2, random);
        ++result.rng_draws;
        if (roll == 0) result.replacement_spawn = config.tiny;
        else if (roll == 1) result.replacement_spawn = config.small;
        else result.replacement_spawn = config.medium;
    } else if (size == fourcc('l', 'a', 'r', 'a')) {
        // 0x16AC0: range 0..1, roll 0 -> Large, roll 1 -> Medium.
        const int roll = choose_inclusive_integer(0, 1, random);
        ++result.rng_draws;
        result.replacement_spawn = roll == 0 ? config.large : config.medium;
    }

    if (result.replacement_spawn && empty_or_none(*result.replacement_spawn)) {
        result.replacement_spawn.reset();
    }
    return result;
}

} // namespace deimos
