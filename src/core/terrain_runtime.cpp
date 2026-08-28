#include "deimos/terrain_runtime.hpp"

#include "deimos/collision_runtime.hpp"

#include <array>
#include <cmath>
#include <string_view>

namespace deimos {
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
    if (!entity.behavior.collides_with_ground_obstacles) return false;
    return obstacles.overlaps(legacy_entity_world_rect(entity));
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
