#pragma once

#include "deimos/unit_behavior.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace deimos {

inline constexpr std::size_t kMaxUnitStates = 20;

class LegacyRandom {
public:
    explicit LegacyRandom(std::uint32_t seed = 1u) : seed_(seed) {}

    [[nodiscard]] std::uint32_t seed() const { return seed_; }
    void seed(std::uint32_t value) { seed_ = value; }

    // PPC 0x553E0: 32-bit multiply/add with natural wraparound, then bits
    // 16..30 as a non-negative 15-bit result.
    [[nodiscard]] std::uint32_t next15();

private:
    std::uint32_t seed_;
};

// Minimal runtime state recovered from the 1.0.6 G_Entity transition path.
// World position/physics/animation live elsewhere; this structure owns only
// the transition bookkeeping that has been binary-confirmed.
struct UnitStateRuntime {
    std::size_t current_state = 0;
    std::uint32_t state_entry_tick = 0;
    int timer_delay = 0;
    std::array<int, kMaxUnitStates> state_entry_counts{};
};

struct StateEntryResult {
    bool counter_threshold_reached = false;
    ResolvedStateAction counter_action;
};

// Original RNG helper 0x553e0 returns a 15-bit value; helper 0x46580 maps it
// to min + random % (max-min+1), with equal bounds returning the bound.
// Reversed endpoints are intentionally preserved: PPC 0x46580 performs signed
// division without normalization, and canonical content contains one such range.
[[nodiscard]] int choose_inclusive_integer(
    int minimum,
    int maximum,
    std::uint32_t random_value);

[[nodiscard]] int choose_inclusive_integer(
    int minimum,
    int maximum,
    LegacyRandom& random);

// PPC 0x465E0 maps the same 15-bit LCG to a single-precision range.  Equal
// endpoints return immediately without consuming RNG.  Reversed endpoints
// are intentionally *not* normalized: the routine adds a signed (max-min)
// fraction to the numerically lower endpoint exactly as the executable does.
[[nodiscard]] float choose_legacy_float(
    float minimum,
    float maximum,
    std::uint32_t random_value);

[[nodiscard]] float choose_legacy_float(
    float minimum,
    float maximum,
    LegacyRandom& random);

// State entry sets the tick, chooses the state's timer delay and increments
// that state's persistent entry-count slot.  Counter thresholds are tested
// immediately after the increment in the original transition routine.
[[nodiscard]] StateEntryResult enter_unit_state(
    UnitStateRuntime& runtime,
    const CompiledUnitBehavior& behavior,
    std::size_t state_index,
    std::uint32_t current_tick,
    std::uint32_t random_value);

// PPC uses equality, not >=.  If a caller skips over the exact target tick,
// the original check does not fire on a later tick.
[[nodiscard]] bool state_timer_due(
    const UnitStateRuntime& runtime,
    std::uint32_t current_tick);

// The range transition path treats exactly 0.0 as disabled, then performs a
// strict measured-distance < configured-range comparison.
[[nodiscard]] bool state_range_transition_due(
    float configured_range,
    float measured_range);

// Counter actions that attempt a state-name transition (including an
// unresolved non-empty label) reset the current state's entry counter before
// the transition attempt.  Empty/No State does not; Delete/Destroy exits the
// entity path without needing the reset.
[[nodiscard]] bool counter_action_resets_entry_count(const ResolvedStateAction& action);
void reset_current_state_entry_count(UnitStateRuntime& runtime);

} // namespace deimos
