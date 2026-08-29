#include "deimos/particle_runtime.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

using namespace deimos;

namespace {

std::uint16_t at(const LegacyRasterSurface& s, int x, int y) {
    return s.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(s.width) +
                    static_cast<std::size_t>(x)];
}

LegacyParticle particle(float x, float y, std::uint32_t q = 0) {
    LegacyParticle p;
    p.active = true;
    p.core_color = 0x7c00;   // red
    p.fringe_color = 0x03e0; // green
    p.blend_amount = q;
    p.x = x;
    p.y = y;
    return p;
}

} // namespace

int main() {
    NamedTable<float> game(149, {"unused", 0.0f});
    game[144] = {"Particle_Gravity", 0.96f};
    game[145] = {"Particle_ColorVariationAdjust", 0.12f};
    game[146] = {"Particle_FringeColorAdjust", 0.6f};
    game[147] = {"Particle_BlendAmountRate_Short", 3.0f};
    game[148] = {"Particle_BlendAmountRate_Long", 1.0f};
    std::string tuning_error;
    const auto tuning = compile_legacy_particle_tuning(game, &tuning_error);
    assert(tuning);
    assert(tuning->velocity_damping == 0.96f);
    assert(tuning->color_variation_adjust == 0.12f);
    assert(tuning->fringe_color_adjust == 0.6f);
    assert(tuning->blend_rate_short == 3);
    assert(tuning->blend_rate_long == 1);
    game[146].first = "wrong";
    assert(!compile_legacy_particle_tuning(game, &tuning_error));

    LegacyRasterSurface surface(20, 20, 0);
    LegacyParticleSystem system;
    system.particles.push_back(particle(4.0f, 5.0f, 0));

    const auto stats = rasterize_legacy_particles(
        std::span<const LegacyParticleSystem>(&system, 1), surface, 20, 20, 0);
    assert(stats.systems_examined == 1);
    assert(stats.active_particles_examined == 1);
    assert(stats.particles_drawn == 1);
    assert(stats.pixels_written == 49);

    // Exact 0x43BA0 7x7 radial transparency bands.
    assert(at(surface, 4, 5) == legacy_blend_rgb555(0, 0x03e0, 22));
    assert(at(surface, 6, 5) == legacy_blend_rgb555(0, 0x03e0, 10));
    assert(at(surface, 6, 6) == legacy_blend_rgb555(0, 0x03e0, 6));
    assert(at(surface, 6, 7) == 0x03e0); // q band, fringe
    assert(at(surface, 7, 7) == 0x7c00); // plus-shaped core
    assert(at(surface, 6, 8) == 0x7c00);
    assert(at(surface, 7, 8) == 0x7c00); // center
    assert(at(surface, 8, 8) == 0x7c00);
    assert(at(surface, 7, 9) == 0x7c00);
    assert(at(surface, 8, 7) == 0x03e0); // adjacent non-core tap

    // The executable has a real center discontinuity: q 0..6 uses q, then
    // q==7 makes the center fully core-colored again via q-7.
    surface = LegacyRasterSurface(20, 20, 0);
    system.particles[0] = particle(4.0f, 5.0f, 7);
    (void)rasterize_legacy_particles(
        std::span<const LegacyParticleSystem>(&system, 1), surface, 20, 20, 0);
    assert(at(surface, 7, 8) == 0x7c00); // center t=0
    assert(at(surface, 7, 7) == legacy_blend_rgb555(0, 0x7c00, 7));
    assert(at(surface, 4, 5) == legacy_blend_rgb555(0, 0x03e0, 29));

    // 0x43C94 subtracts the horizontal view offset before clipping/fctiwz.
    surface = LegacyRasterSurface(20, 20, 0);
    system.particles[0] = particle(12.75f, 2.9f, 0);
    const auto shifted = rasterize_legacy_particles(
        std::span<const LegacyParticleSystem>(&system, 1), surface, 20, 20, 4);
    assert(shifted.particles_drawn == 1);
    assert(at(surface, 8, 2) == legacy_blend_rgb555(0, 0x03e0, 22));

    // Exact float clip uses x/y>=0 and x/y+7 < visible extent before trunc.
    system.particles = {particle(-0.1f, 1.0f), particle(13.0f, 1.0f), particle(12.0f, 13.0f)};
    surface = LegacyRasterSurface(20, 20, 0);
    const auto clipped = rasterize_legacy_particles(
        std::span<const LegacyParticleSystem>(&system, 1), surface, 20, 20, 0);
    assert(clipped.active_particles_examined == 3);
    assert(clipped.particles_drawn == 0);
    assert(clipped.pixels_written == 0);

    // Positive +0x468 render delay skips the entire particle object.
    system.render_delay = 1;
    system.particles = {particle(1.0f, 1.0f)};
    const auto delayed = rasterize_legacy_particles(
        std::span<const LegacyParticleSystem>(&system, 1), surface, 20, 20, 0);
    assert(delayed.systems_examined == 1);
    assert(delayed.active_particles_examined == 0);
    assert(delayed.particles_drawn == 0);

    std::cout << "particle_runtime_test: PASS\n";
    return 0;
}
