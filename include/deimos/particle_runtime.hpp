#pragma once

#include "deimos/data_tables.hpp"
#include "deimos/render_backend.hpp"
#include "deimos/state_runtime.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace deimos {

// Game[gafl] indices 144..148, read by the 0x43340/0x438C0 particle cluster.
// The original name "Particle_Gravity" is retained even though 0x438C0
// multiplies both velocity components by it, making it a damping factor.
struct LegacyParticleTuning {
    float velocity_damping = 0.0f;
    float color_variation_adjust = 0.0f;
    float fringe_color_adjust = 0.0f;
    int blend_rate_short = 0;
    int blend_rate_long = 0;
};

[[nodiscard]] std::optional<LegacyParticleTuning> compile_legacy_particle_tuning(
    const NamedTable<float>& game_floats,
    std::string* error = nullptr);

// Clean semantic form of the 24-byte request consumed by PPC 0x43340.
// All three recovered producers build this exact shape before calling it.
struct LegacyParticleSpawnRequest {
    float x = 0.0f;                    // +0x00
    float y = 0.0f;                    // +0x04
    std::uint16_t color = 0;           // +0x08, xRGB1555
    int delay = 0;                     // +0x0C
    bool ground_space = false;         // +0x10: add 0xFED0 terrain delta to Y
    FourCC preset{};                   // +0x14
};

struct LegacyParticleDirection {
    float x = 0.0f;
    float y = 0.0f;
};

// PPC 0x44630 generates both 100-entry tables once at subsystem startup.
// varied contains the 1/.85/.70/.55 random speed multipliers used by the
// non-*ci presets; circular contains the corresponding unit directions.
struct LegacyParticleDirectionState {
    std::array<LegacyParticleDirection, 100> varied{};
    std::array<LegacyParticleDirection, 100> circular{};
    int varied_cursor = 0;             // original global r2-24772
    int circular_cursor = 0;           // original global r2-24776
    bool initialized = false;
};

// Clean semantic form of one 28-byte particle record consumed by PPC 0x43BA0.
struct LegacyParticle {
    bool active = false;                 // +0x00
    std::uint16_t core_color = 0;        // +0x02, xRGB1555
    std::uint16_t fringe_color = 0;      // +0x04, xRGB1555
    std::uint32_t blend_amount = 0;      // +0x08
    float x = 0.0f;                      // +0x0C
    float y = 0.0f;                      // +0x10
    float velocity_x = 0.0f;             // +0x14
    float velocity_y = 0.0f;             // +0x18
};

// 0x43340 allocates a fixed 0x470-byte object. These five clean fields are the
// recovered system bytes/words at +0x464..+0x46C. reverse_blend is initialized
// false by every recovered producer but is preserved because 0x438C0 contains
// a complete alternate fade branch for it.
struct LegacyParticleSystem {
    bool ground_space = false;           // +0x464
    bool reverse_blend = false;          // +0x465
    bool short_velocity = false;         // +0x466
    bool circular_velocity = false;      // +0x467
    int render_delay = 0;                // +0x468
    std::vector<LegacyParticle> particles; // active legacy count at +0x46C
};

struct LegacyParticleRuntime {
    LegacyParticleDirectionState directions;
    std::vector<LegacyParticleSystem> systems;
};

// Optional bridge used by gameplay producers that call 0x43340 inline in the
// original executable. A null/incomplete context keeps bounded headless tests
// side-effect free; a complete context preserves the producer's original RNG
// ordering by constructing the particle system at the exact call site.
struct LegacyParticleExecutionContext {
    LegacyParticleRuntime* runtime = nullptr;
    const LegacyParticleTuning* tuning = nullptr;

    [[nodiscard]] bool valid() const { return runtime != nullptr && tuning != nullptr; }
};

// All three recovered gameplay producers construct the same 24-byte request.
// Source-format colors are 24-bit RGB; the compiled executable stores the
// request color as xRGB1555.
[[nodiscard]] LegacyParticleSpawnRequest make_legacy_particle_spawn_request(
    float x,
    float y,
    FourCC preset,
    Rgb24 color,
    bool ground_space,
    int delay = 0);

// Exact startup-side semantic counterpart of 0x44630 plus 0x431F0's two
// cursor seeds. This consumes 302 RNG draws for non-zero width/height:
// 3 per direction (X, Y, varied-speed selector) and two 0..99 cursor draws.
void initialize_legacy_particle_directions(
    LegacyParticleRuntime& runtime,
    int visible_width,
    int visible_height,
    LegacyRandom& random);

// Maps the eight accepted FourCC presets to the exact 0x43340 count/flag
// contract and emits one particle system. Returns false for zero, 'none', or
// any unrecognized preset and does not consume RNG in that case.
[[nodiscard]] bool spawn_legacy_particle_system(
    LegacyParticleRuntime& runtime,
    const LegacyParticleSpawnRequest& request,
    const LegacyParticleTuning& tuning,
    LegacyRandom& random);

[[nodiscard]] bool execute_legacy_particle_spawn(
    LegacyParticleExecutionContext execution,
    const LegacyParticleSpawnRequest& request,
    LegacyRandom& random);

struct LegacyParticleUpdateStats {
    std::size_t systems_examined = 0;
    std::size_t systems_delayed = 0;
    std::size_t systems_removed = 0;
    std::size_t particles_examined = 0;
    std::size_t particles_deactivated = 0;
    std::size_t active_particles_after = 0;
};

// Exact clean counterpart of PPC 0x438C0. Every system delay is decremented
// first; a result >0 skips that system for the tick. Ground-space systems add
// 0xFED0's applied terrain delta to Y, both velocities are damped, position is
// integrated, bounds/fade lifetime is applied, and empty systems are removed.
[[nodiscard]] LegacyParticleUpdateStats update_legacy_particles(
    LegacyParticleRuntime& runtime,
    const LegacyParticleTuning& tuning,
    int visible_width,
    int visible_height,
    int applied_vertical_scroll_delta);

struct LegacyParticleRasterStats {
    std::size_t systems_examined = 0;
    std::size_t active_particles_examined = 0;
    std::size_t particles_drawn = 0;
    std::size_t pixels_written = 0;
};

// Exact clean counterpart of PPC 0x43BA0. It directly mutates the visible
// 16-bit xRGB1555 surface after queued sprite layers 2..5 and before 6..15.
// Particle X is converted to screen space by subtracting 0x100A0's horizontal
// view offset; Y is already in visible-game coordinates. The executable clips
// using float tests x/y>=0 and x/y+7<VisibleGameWidth/Height before fctiwz,
// then writes the complete 7x7 kernel without per-pixel clipping.
[[nodiscard]] LegacyParticleRasterStats rasterize_legacy_particles(
    std::span<const LegacyParticleSystem> systems,
    LegacyRasterSurface& visible_surface,
    int visible_width,
    int visible_height,
    int horizontal_view_offset);

} // namespace deimos
