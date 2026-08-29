#include "deimos/particle_runtime.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace deimos {
namespace {

constexpr FourCC fourcc(char a, char b, char c, char d) {
    return FourCC{{a, b, c, d}};
}

constexpr FourCC kNone = fourcc('n','o','n','e');
constexpr FourCC kTiny = fourcc('t','i','n','y');
constexpr FourCC kSmall = fourcc('s','m','a','l');
constexpr FourCC kMedium = fourcc('m','e','d',' ');
constexpr FourCC kLarge = fourcc('l','a','r','g');
constexpr FourCC kTinyCircular = fourcc('t','i','c','i');
constexpr FourCC kSmallCircular = fourcc('s','m','c','i');
constexpr FourCC kMediumCircular = fourcc('m','e','c','i');
constexpr FourCC kLargeCircular = fourcc('l','a','c','i');

struct PresetInfo {
    int count = 0;
    bool short_velocity = false;
    bool circular = false;
};

std::optional<PresetInfo> preset_info(FourCC preset) {
    if (preset == kTiny) return PresetInfo{5, true, false};
    if (preset == kSmall) return PresetInfo{10, true, false};
    if (preset == kMedium) return PresetInfo{20, false, false};
    if (preset == kLarge) return PresetInfo{40, false, false};
    if (preset == kTinyCircular) return PresetInfo{5, true, true};
    if (preset == kSmallCircular) return PresetInfo{10, true, true};
    if (preset == kMediumCircular) return PresetInfo{20, false, true};
    if (preset == kLargeCircular) return PresetInfo{40, false, true};
    return std::nullopt;
}

bool is_zero_fourcc(FourCC id) {
    return id.bytes == std::array<char, 4>{};
}

int clamp31(unsigned value) {
    return static_cast<int>(std::min(value, 31u));
}

void blend_pixel(
    LegacyRasterSurface& surface,
    int x,
    int y,
    std::uint16_t color,
    int transparency) {
    auto& pixel = surface.pixels[
        static_cast<std::size_t>(y) * static_cast<std::size_t>(surface.width) +
        static_cast<std::size_t>(x)];
    pixel = legacy_blend_rgb555(pixel, color, transparency);
}

float ppc_f32(float value) {
    // Named helper makes the intentional single-precision round points in the
    // recovered PPC arithmetic visible in the clean implementation.
    return static_cast<float>(value);
}

std::uint16_t rgb16_components_to_rgb555(
    std::uint16_t red,
    std::uint16_t green,
    std::uint16_t blue) {
    return static_cast<std::uint16_t>(
        ((red >> 11u) << 10u) |
        ((green >> 11u) << 5u) |
        (blue >> 11u));
}

std::array<std::uint16_t, 5> build_color_palette(
    std::uint16_t source,
    float color_variation,
    float fringe_adjust,
    bool fringe) {
    const std::array<unsigned, 3> channels = {
        static_cast<unsigned>((source >> 10u) & 31u),
        static_cast<unsigned>((source >> 5u) & 31u),
        static_cast<unsigned>(source & 31u),
    };

    std::array<std::uint16_t, 5> out{};
    for (int variant = 0; variant < 5; ++variant) {
        const float dim = ppc_f32(1.0f - ppc_f32(static_cast<float>(variant) * color_variation));
        std::array<std::uint16_t, 3> expanded{};
        for (std::size_t c = 0; c < channels.size(); ++c) {
            // 0x43588..0x43670: expand the 5-bit source channel through
            // 65535*(channel/32), then apply the variant factor. The fringe
            // copy is subsequently multiplied by Particle_FringeColorAdjust.
            const float unit = ppc_f32(static_cast<float>(channels[c]) * 0.03125f);
            const int base = static_cast<int>(std::trunc(ppc_f32(65535.0f * unit)));
            int value = static_cast<int>(std::trunc(ppc_f32(static_cast<float>(base) * dim)));
            if (fringe) {
                value = static_cast<int>(
                    std::trunc(ppc_f32(static_cast<float>(static_cast<std::uint16_t>(value)) * fringe_adjust)));
            }
            expanded[c] = static_cast<std::uint16_t>(value);
        }
        out[static_cast<std::size_t>(variant)] = rgb16_components_to_rgb555(
            expanded[0], expanded[1], expanded[2]);
    }
    return out;
}

float legacy_sqrt_quantized(int squared) {
    // 0x42F20 is table-backed below 16384 and sqrt/frsp above it. Both paths
    // return the single-precision sqrt of the already-fctiwz integer input.
    return static_cast<float>(std::sqrt(static_cast<double>(squared)));
}

LegacyParticleDirection generate_unit_direction(
    int visible_width,
    int visible_height,
    LegacyRandom& random) {
    const float center_x = ppc_f32(static_cast<float>(visible_width) * 0.5f);
    const float center_y = ppc_f32(static_cast<float>(visible_height) * 0.5f);
    const int random_x = choose_inclusive_integer(0, visible_width, random);
    const int random_y = choose_inclusive_integer(0, visible_height, random);
    const float dx = ppc_f32(center_x - static_cast<float>(random_x));
    const float dy = ppc_f32(center_y - static_cast<float>(random_y));
    const float squared_f = ppc_f32(ppc_f32(dx * dx) + ppc_f32(dy * dy));
    const int squared = static_cast<int>(std::trunc(squared_f));
    const float magnitude = legacy_sqrt_quantized(squared);
    return {
        ppc_f32(dx / magnitude),
        ppc_f32(dy / magnitude),
    };
}

LegacyParticleDirection scaled_direction(
    LegacyParticleDirection direction,
    int selector) {
    double scale = 1.0;
    if (selector == 1) scale = 0.85;
    else if (selector == 2) scale = 0.70;
    else if (selector == 3) scale = 0.55;
    direction.x = static_cast<float>(static_cast<double>(direction.x) * scale);
    direction.y = static_cast<float>(static_cast<double>(direction.y) * scale);
    return direction;
}

LegacyParticleDirection take_direction(
    LegacyParticleDirectionState& directions,
    bool circular) {
    int& cursor = circular ? directions.circular_cursor : directions.varied_cursor;
    const auto& table = circular ? directions.circular : directions.varied;
    const int index = std::clamp(cursor, 0, 99);
    const auto result = table[static_cast<std::size_t>(index)];

    // 0x437E4..0x43830 increments, then resets when the result is >=99.
    // Therefore normal cycling is 0..98; index 99 can occur only when startup
    // initially seeds the cursor to 99.
    ++cursor;
    if (cursor >= 99) cursor = 0;
    return result;
}

} // namespace

std::optional<LegacyParticleTuning> compile_legacy_particle_tuning(
    const NamedTable<float>& game_floats,
    std::string* error) {
    constexpr std::size_t kFirst = 144;
    constexpr std::array<std::string_view, 5> labels = {
        "Particle_Gravity",
        "Particle_ColorVariationAdjust",
        "Particle_FringeColorAdjust",
        "Particle_BlendAmountRate_Short",
        "Particle_BlendAmountRate_Long",
    };
    if (game_floats.size() < kFirst + labels.size()) {
        if (error) *error = "Game[gafl] is shorter than the 1.0.6 particle positional contract";
        return std::nullopt;
    }
    for (std::size_t i = 0; i < labels.size(); ++i) {
        if (game_floats[kFirst + i].first != labels[i]) {
            if (error) *error = "unexpected Game[gafl] particle label at index " +
                std::to_string(kFirst + i);
            return std::nullopt;
        }
    }
    LegacyParticleTuning out;
    out.velocity_damping = game_floats[144].second;
    out.color_variation_adjust = game_floats[145].second;
    out.fringe_color_adjust = game_floats[146].second;
    out.blend_rate_short = static_cast<int>(std::trunc(game_floats[147].second));
    out.blend_rate_long = static_cast<int>(std::trunc(game_floats[148].second));
    if (out.blend_rate_short < 0 || out.blend_rate_long < 0) {
        if (error) *error = "particle blend rates must be non-negative";
        return std::nullopt;
    }
    return out;
}

LegacyParticleSpawnRequest make_legacy_particle_spawn_request(
    float x,
    float y,
    FourCC preset,
    Rgb24 color,
    bool ground_space,
    int delay) {
    LegacyParticleSpawnRequest request;
    request.x = x;
    request.y = y;
    request.color = static_cast<std::uint16_t>(
        ((static_cast<std::uint16_t>(color.red) >> 3u) << 10u) |
        ((static_cast<std::uint16_t>(color.green) >> 3u) << 5u) |
         (static_cast<std::uint16_t>(color.blue) >> 3u));
    request.delay = delay;
    request.ground_space = ground_space;
    request.preset = preset;
    return request;
}

void initialize_legacy_particle_directions(
    LegacyParticleRuntime& runtime,
    int visible_width,
    int visible_height,
    LegacyRandom& random) {
    for (std::size_t i = 0; i < runtime.directions.circular.size(); ++i) {
        const auto unit = generate_unit_direction(visible_width, visible_height, random);
        runtime.directions.circular[i] = unit;
        const int selector = choose_inclusive_integer(0, 3, random);
        runtime.directions.varied[i] = scaled_direction(unit, selector);
    }
    runtime.directions.varied_cursor = choose_inclusive_integer(0, 99, random);
    runtime.directions.circular_cursor = choose_inclusive_integer(0, 99, random);
    runtime.directions.initialized = true;
}

bool spawn_legacy_particle_system(
    LegacyParticleRuntime& runtime,
    const LegacyParticleSpawnRequest& request,
    const LegacyParticleTuning& tuning,
    LegacyRandom& random) {
    if (is_zero_fourcc(request.preset) || request.preset == kNone) return false;
    const auto info = preset_info(request.preset);
    if (!info) return false;

    LegacyParticleSystem system;
    system.ground_space = request.ground_space;
    system.reverse_blend = false;
    system.short_velocity = info->short_velocity;
    system.circular_velocity = info->circular;
    system.render_delay = request.delay;
    system.particles.reserve(static_cast<std::size_t>(info->count));

    const auto core_palette = build_color_palette(
        request.color, tuning.color_variation_adjust, tuning.fringe_color_adjust, false);
    const auto fringe_palette = build_color_palette(
        request.color, tuning.color_variation_adjust, tuning.fringe_color_adjust, true);
    const float velocity_scale = info->short_velocity ? 3.0f : 5.0f;

    for (int i = 0; i < info->count; ++i) {
        LegacyParticle particle;
        particle.active = true;
        particle.x = request.x;
        particle.y = request.y;
        const int color_variant = choose_inclusive_integer(0, 4, random);
        particle.core_color = core_palette[static_cast<std::size_t>(color_variant)];
        particle.fringe_color = fringe_palette[static_cast<std::size_t>(color_variant)];
        particle.blend_amount = 0;

        const auto direction = take_direction(runtime.directions, info->circular);
        particle.velocity_x = ppc_f32(direction.x * velocity_scale);
        particle.velocity_y = ppc_f32(direction.y * velocity_scale);
        system.particles.push_back(particle);
    }

    runtime.systems.push_back(std::move(system));
    return true;
}

bool execute_legacy_particle_spawn(
    LegacyParticleExecutionContext execution,
    const LegacyParticleSpawnRequest& request,
    LegacyRandom& random) {
    if (!execution.valid()) return false;
    return spawn_legacy_particle_system(
        *execution.runtime, request, *execution.tuning, random);
}

LegacyParticleUpdateStats update_legacy_particles(
    LegacyParticleRuntime& runtime,
    const LegacyParticleTuning& tuning,
    int visible_width,
    int visible_height,
    int applied_vertical_scroll_delta) {
    LegacyParticleUpdateStats stats;
    const float x_min = -32.0f;
    const float footprint = 7.0f;
    const float x_max_extent = static_cast<float>(visible_width + 32);
    const float y_max_extent = static_cast<float>(visible_height);
    const auto blend_rate = static_cast<std::uint32_t>(std::max(0, tuning.blend_rate_long));

    for (std::size_t system_index = 0; system_index < runtime.systems.size();) {
        auto& system = runtime.systems[system_index];
        ++stats.systems_examined;

        --system.render_delay;
        if (system.render_delay > 0) {
            ++stats.systems_delayed;
            ++system_index;
            continue;
        }

        bool any_active = false;
        for (auto& particle : system.particles) {
            if (!particle.active) continue;
            ++stats.particles_examined;

            if (system.ground_space) {
                particle.y = ppc_f32(
                    particle.y + static_cast<float>(applied_vertical_scroll_delta));
            }

            particle.velocity_x = ppc_f32(particle.velocity_x * tuning.velocity_damping);
            particle.velocity_y = ppc_f32(particle.velocity_y * tuning.velocity_damping);
            particle.x = ppc_f32(particle.x + particle.velocity_x);
            particle.y = ppc_f32(particle.y + particle.velocity_y);

            const bool in_bounds =
                particle.x >= x_min &&
                ppc_f32(particle.x + footprint) <= x_max_extent &&
                particle.y >= 0.0f &&
                ppc_f32(particle.y + footprint) <= y_max_extent;

            if (!in_bounds && !system.reverse_blend) {
                particle.active = false;
                ++stats.particles_deactivated;
                continue;
            }

            if (!system.reverse_blend) {
                if (particle.blend_amount < 32u) {
                    particle.blend_amount += blend_rate;
                    if (particle.blend_amount > 32u) particle.blend_amount = 32u;
                } else {
                    particle.active = false;
                    ++stats.particles_deactivated;
                    continue;
                }
            } else {
                if (particle.blend_amount != 0u) {
                    if (particle.blend_amount <= blend_rate) particle.blend_amount = 0u;
                    else particle.blend_amount -= blend_rate;
                } else {
                    particle.active = false;
                    ++stats.particles_deactivated;
                    continue;
                }
            }

            if (particle.active) {
                any_active = true;
                ++stats.active_particles_after;
            }
        }

        if (!any_active) {
            runtime.systems.erase(runtime.systems.begin() + static_cast<std::ptrdiff_t>(system_index));
            ++stats.systems_removed;
            continue;
        }
        ++system_index;
    }
    return stats;
}

LegacyParticleRasterStats rasterize_legacy_particles(
    std::span<const LegacyParticleSystem> systems,
    LegacyRasterSurface& visible_surface,
    int visible_width,
    int visible_height,
    int horizontal_view_offset) {
    LegacyParticleRasterStats stats;
    if (!visible_surface.valid() || visible_width <= 0 || visible_height <= 0 ||
        visible_surface.width < visible_width || visible_surface.height < visible_height) {
        return stats;
    }

    static constexpr int kBandOffset[7][7] = {
        {22, 22, 10, 10, 10, 22, 22},
        {22, 10,  6,  6,  6, 10, 22},
        {10,  6,  0,  0,  0,  6, 10},
        {10,  6,  0, -1,  0,  6, 10},
        {10,  6,  0,  0,  0,  6, 10},
        {22, 10,  6,  6,  6, 10, 22},
        {22, 22, 10, 10, 10, 22, 22},
    };
    static constexpr bool kCore[7][7] = {
        {false,false,false,false,false,false,false},
        {false,false,false,false,false,false,false},
        {false,false,false,true ,false,false,false},
        {false,false,true ,true ,true ,false,false},
        {false,false,false,true ,false,false,false},
        {false,false,false,false,false,false,false},
        {false,false,false,false,false,false,false},
    };

    for (const auto& system : systems) {
        ++stats.systems_examined;
        if (system.render_delay > 0) continue;

        for (const auto& particle : system.particles) {
            if (!particle.active) continue;
            ++stats.active_particles_examined;

            const float screen_x = particle.x - static_cast<float>(horizontal_view_offset);
            const float screen_y = particle.y;
            if (!(screen_x >= 0.0f) || !(screen_x + 7.0f < static_cast<float>(visible_width)) ||
                !(screen_y >= 0.0f) || !(screen_y + 7.0f < static_cast<float>(visible_height))) {
                continue;
            }

            const int left = static_cast<int>(std::trunc(screen_x));
            const int top = static_cast<int>(std::trunc(screen_y));
            const unsigned q = particle.blend_amount;
            const int center = q > 6u ? static_cast<int>(q - 7u) : static_cast<int>(q);

            for (int row = 0; row < 7; ++row) {
                for (int col = 0; col < 7; ++col) {
                    const int marker = kBandOffset[row][col];
                    const int transparency = marker < 0
                        ? center
                        : (marker == 0 ? static_cast<int>(q) : clamp31(q + static_cast<unsigned>(marker)));
                    blend_pixel(
                        visible_surface,
                        left + col,
                        top + row,
                        kCore[row][col] ? particle.core_color : particle.fringe_color,
                        transparency);
                    ++stats.pixels_written;
                }
            }
            ++stats.particles_drawn;
        }
    }
    return stats;
}

} // namespace deimos
