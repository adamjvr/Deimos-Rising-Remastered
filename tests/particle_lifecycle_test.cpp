#include "deimos/entity_runtime.hpp"
#include "deimos/particle_runtime.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>

using namespace deimos;

namespace {

FourCC id(const char (&text)[5]) {
    return FourCC{{text[0], text[1], text[2], text[3]}};
}

DefinitionField f_bool(const char* key, bool value) {
    return {key, value, value ? "TRUE" : "FALSE", 0};
}
DefinitionField f_int(const char* key, int value) {
    return {key, value, std::to_string(value), 0};
}
DefinitionField f_id(const char* key, FourCC value) {
    return {key, value, value.str(), 0};
}
DefinitionField f_color(const char* key, Rgb24 value) {
    return {key, value, std::to_string(value.red) + "," +
        std::to_string(value.green) + "," + std::to_string(value.blue), 0};
}

bool near(float a, float b, float epsilon = 1.0e-5f) {
    return std::fabs(a - b) <= epsilon;
}

UnitDefinition particle_unit(bool repeat, int repeat_delay, int max_bursts) {
    UnitDefinition unit;
    unit.core_fields.add(f_bool("isGroundBased_BOOL", true));
    UnitStateDefinition state;
    state.name = "Particle";
    state.fields.add(f_id("stateParticles_ID", id("tiny")));
    state.fields.add(f_color("stateParticlesColor_COLOR", {248, 128, 64}));
    state.fields.add(f_bool("stateParticlesRepeat_BOOL", repeat));
    state.fields.add(f_int("stateParticles_RepeatDelay_INT", repeat_delay));
    state.fields.add(f_int("stateParticles_MaxNumBursts_INT", max_bursts));
    unit.states.push_back(std::move(state));
    return unit;
}

LegacyParticleTuning tuning() {
    LegacyParticleTuning t;
    t.velocity_damping = 0.96f;
    t.color_variation_adjust = 0.12f;
    t.fringe_color_adjust = 0.6f;
    t.blend_rate_short = 3;
    t.blend_rate_long = 1;
    return t;
}

} // namespace

int main() {
    // Independent startup oracle from PPC 0x44630/0x431F0. Starting from
    // 0x12345678 at 416x480 consumes 302 LCG draws and lands here.
    LegacyParticleRuntime runtime;
    LegacyRandom init_rng(0x12345678u);
    initialize_legacy_particle_directions(runtime, 416, 480, init_rng);
    assert(init_rng.seed() == 0x3af362fau);
    assert(runtime.directions.varied_cursor == 27);
    assert(runtime.directions.circular_cursor == 91);
    assert(near(runtime.directions.circular[0].x, 0.8320502639f));
    assert(near(runtime.directions.circular[0].y, 0.5547001958f));
    assert(near(runtime.directions.varied[0].x, 0.7072427273f));
    assert(near(runtime.directions.varied[0].y, 0.4714951515f));
    assert(near(runtime.directions.circular[1].x, -0.4880787134f));
    assert(near(runtime.directions.varied[2].x, -0.7944661975f));

    // All gameplay producers convert RGB24 to xRGB1555 before 0x43340.
    const auto request = make_legacy_particle_spawn_request(
        12.5f, 20.25f, id("tiny"), {248, 128, 64}, true, 7);
    assert(request.x == 12.5f && request.y == 20.25f);
    assert(request.color == 0x7e08u);
    assert(request.ground_space && request.delay == 7 && request.preset == id("tiny"));

    // 0x43340 preset map and the cursor-99 startup quirk. Unknown presets do
    // not allocate and consume no RNG.
    LegacyParticleRuntime spawn_runtime;
    for (auto& d : spawn_runtime.directions.varied) d = {1.0f, 0.0f};
    for (auto& d : spawn_runtime.directions.circular) d = {0.0f, 1.0f};
    spawn_runtime.directions.initialized = true;
    spawn_runtime.directions.varied_cursor = 99;
    LegacyRandom spawn_rng(123u);
    const auto seed_before_unknown = spawn_rng.seed();
    auto unknown = request;
    unknown.preset = id("xxxx");
    assert(!spawn_legacy_particle_system(spawn_runtime, unknown, tuning(), spawn_rng));
    assert(spawn_rng.seed() == seed_before_unknown);
    assert(spawn_runtime.systems.empty());

    auto tiny = request;
    tiny.delay = 0;
    assert(spawn_legacy_particle_system(spawn_runtime, tiny, tuning(), spawn_rng));
    assert(spawn_runtime.systems.size() == 1);
    assert(spawn_runtime.systems[0].particles.size() == 5);
    assert(spawn_runtime.systems[0].short_velocity);
    assert(!spawn_runtime.systems[0].circular_velocity);
    assert(spawn_runtime.directions.varied_cursor == 4); // 99 then 0,1,2,3
    assert(spawn_rng.seed() == 0x44284c44u); // five color-variant draws
    assert(near(spawn_runtime.systems[0].particles[0].velocity_x, 3.0f));

    // 0x438C0: delay decrement, ground-scroll adjustment, velocity damping,
    // integration and the LONG blend rate. Upper footprint equality is valid.
    LegacyParticleRuntime update_runtime;
    LegacyParticleSystem update_system;
    update_system.ground_space = true;
    update_system.render_delay = 2;
    LegacyParticle p;
    p.active = true;
    p.x = 0.0f; p.y = 10.0f;
    p.velocity_x = 1.0f; p.velocity_y = -2.0f;
    update_system.particles.push_back(p);
    update_runtime.systems.push_back(update_system);
    auto stats = update_legacy_particles(update_runtime, tuning(), 416, 480, -1);
    assert(stats.systems_delayed == 1);
    assert(update_runtime.systems[0].particles[0].x == 0.0f);
    stats = update_legacy_particles(update_runtime, tuning(), 416, 480, -1);
    const auto& moved = update_runtime.systems[0].particles[0];
    assert(near(moved.velocity_x, 0.96f));
    assert(near(moved.velocity_y, -1.92f));
    assert(near(moved.x, 0.96f));
    assert(near(moved.y, 7.08f));
    assert(moved.blend_amount == 1u);

    LegacyParticleRuntime edge_runtime;
    LegacyParticleSystem edge_system;
    LegacyParticle edge;
    edge.active = true;
    edge.x = 441.0f; // 441+7 == 416+32: accepted
    edge.y = 473.0f; // 473+7 == 480: accepted
    edge.blend_amount = 31;
    edge_system.particles.push_back(edge);
    edge_runtime.systems.push_back(edge_system);
    stats = update_legacy_particles(edge_runtime, tuning(), 416, 480, 0);
    assert(edge_runtime.systems.size() == 1);
    assert(edge_runtime.systems[0].particles[0].blend_amount == 32u);
    stats = update_legacy_particles(edge_runtime, tuning(), 416, 480, 0);
    assert(edge_runtime.systems.empty());
    assert(stats.systems_removed == 1 && stats.particles_deactivated == 1);

    // State producer runs before timer/rules. With an execution context it
    // consumes particle RNG inline; without one it still updates +0xF0/+0xF4.
    auto unit = particle_unit(false, 0, 0);
    EntityRuntime entity;
    entity.x = 33.0f; entity.y = 44.0f;
    LegacyRandom entity_rng(7u);
    initialize_entity_state_machine(entity, unit, 0, entity_rng);
    assert(entity.state_particle_burst_count == 0);
    EntityTickContext ctx;
    ctx.current_tick = 10;
    auto tick = advance_entity_runtime(entity, unit, ctx, entity_rng);
    assert(tick.state_particle_spawn);
    assert(!tick.state_particle_executed);
    assert(tick.state_particle_spawn->x == 33.0f && tick.state_particle_spawn->y == 44.0f);
    assert(tick.state_particle_spawn->ground_space);
    assert(entity.state_particle_burst_count == 1);
    assert(entity.last_state_particle_tick == 10);
    ctx.current_tick = 11;
    tick = advance_entity_runtime(entity, unit, ctx, entity_rng);
    assert(!tick.state_particle_spawn);

    // Re-entering a state resets only +0xF4, not +0xF0.
    enter_entity_state(entity, unit, 0, 12, entity_rng);
    assert(entity.state_particle_burst_count == 0);
    assert(entity.last_state_particle_tick == 10);
    ctx.current_tick = 12;
    tick = advance_entity_runtime(entity, unit, ctx, entity_rng);
    assert(tick.state_particle_spawn);

    // Repeat delay uses last_tick + delay with equality due; max bursts gates
    // after the repeat test. Re-entry resets count while preserving timestamp.
    auto repeat_unit = particle_unit(true, 5, 2);
    EntityRuntime repeating;
    LegacyRandom repeat_rng(1u);
    initialize_entity_state_machine(repeating, repeat_unit, 0, repeat_rng);
    EntityTickContext repeat_ctx;
    repeat_ctx.current_tick = 10;
    assert(advance_entity_runtime(repeating, repeat_unit, repeat_ctx, repeat_rng).state_particle_spawn);
    repeat_ctx.current_tick = 14;
    assert(!advance_entity_runtime(repeating, repeat_unit, repeat_ctx, repeat_rng).state_particle_spawn);
    repeat_ctx.current_tick = 15;
    assert(advance_entity_runtime(repeating, repeat_unit, repeat_ctx, repeat_rng).state_particle_spawn);
    repeat_ctx.current_tick = 20;
    assert(!advance_entity_runtime(repeating, repeat_unit, repeat_ctx, repeat_rng).state_particle_spawn);
    enter_entity_state(repeating, repeat_unit, 0, 21, repeat_rng);
    assert(repeating.state_particle_burst_count == 0);
    assert(repeating.last_state_particle_tick == 15);
    repeat_ctx.current_tick = 21;
    assert(advance_entity_runtime(repeating, repeat_unit, repeat_ctx, repeat_rng).state_particle_spawn);

    // Exact inline execution proof: no timer/spawn-set RNG occurs here, so a
    // tiny burst from seed 123 must consume exactly five color draws.
    auto exec_unit = particle_unit(false, 0, 0);
    EntityRuntime executing;
    LegacyRandom exec_rng(123u);
    initialize_entity_state_machine(executing, exec_unit, 0, exec_rng);
    LegacyParticleRuntime exec_runtime;
    for (auto& d : exec_runtime.directions.varied) d = {1.0f, 0.0f};
    exec_runtime.directions.initialized = true;
    LegacyParticleTuning exec_tuning = tuning();
    EntityTickContext exec_ctx;
    exec_ctx.current_tick = 1;
    exec_ctx.particle_execution = {&exec_runtime, &exec_tuning};
    tick = advance_entity_runtime(executing, exec_unit, exec_ctx, exec_rng);
    assert(tick.state_particle_executed);
    assert(exec_runtime.systems.size() == 1);
    assert(exec_runtime.systems[0].particles.size() == 5);
    assert(exec_rng.seed() == 0x44284c44u);

    std::cout << "particle_lifecycle_test: PASS\n";
    return 0;
}
