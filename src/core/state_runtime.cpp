#include "deimos/state_runtime.hpp"

#include <stdexcept>
#include <cstdint>

namespace deimos {

std::uint32_t LegacyRandom::next15() {
    seed_ = seed_ * 1103515245u + 12345u;
    return (seed_ >> 16u) & 0x7FFFu;
}

int choose_inclusive_integer(int minimum, int maximum, std::uint32_t random_value) {
    if (minimum == maximum) return minimum;

    // PPC 0x46580 does not normalize or validate endpoints.  It computes a
    // signed divisor (maximum - minimum + 1), performs divw, reconstructs the
    // signed remainder, then adds minimum.  Canonical 1.0.6 data contains one
    // reversed spawn-rate range (110..20), so preserving this behavior is
    // required for fidelity.
    const auto width = static_cast<std::int32_t>(
        static_cast<std::uint32_t>(maximum) - static_cast<std::uint32_t>(minimum) + 1u);
    if (width == 0) {
        throw std::invalid_argument("inclusive random range produces zero PPC divisor");
    }
    const auto value = static_cast<std::int32_t>(random_value);
    const auto quotient = value / width; // C++ truncates toward zero like PPC divw.
    const auto remainder = value - quotient * width;
    return minimum + remainder;
}

int choose_inclusive_integer(int minimum, int maximum, LegacyRandom& random) {
    if (minimum == maximum) return minimum; // original does not consume RNG here
    return choose_inclusive_integer(minimum, maximum, random.next15());
}

float choose_legacy_float(float minimum, float maximum, std::uint32_t random_value) {
    if (minimum == maximum) return minimum;

    // PPC 0x465E0:
    //   low = min(minimum, maximum)
    //   span = frsp(maximum - minimum)   // sign deliberately preserved
    //   scaled = fmuls(span, float(rng15))
    //   fraction = fdivs(scaled, 32767.0f)
    //   result = fadds(fraction, low)
    // Keep explicit single-precision round points instead of normalizing the
    // endpoints into a conventional random range.
    const float low = minimum <= maximum ? minimum : maximum;
    const float span = static_cast<float>(maximum - minimum);
    const float draw = static_cast<float>(static_cast<std::int32_t>(random_value));
    const float scaled = static_cast<float>(span * draw);
    const float fraction = static_cast<float>(scaled / 32767.0f);
    return static_cast<float>(fraction + low);
}

float choose_legacy_float(float minimum, float maximum, LegacyRandom& random) {
    if (minimum == maximum) return minimum;
    return choose_legacy_float(minimum, maximum, random.next15());
}

StateEntryResult enter_unit_state(
    UnitStateRuntime& runtime,
    const CompiledUnitBehavior& behavior,
    std::size_t state_index,
    std::uint32_t current_tick,
    std::uint32_t random_value) {
    if (state_index >= behavior.states.size() || state_index >= kMaxUnitStates) {
        throw std::out_of_range("unit state index outside compiled/original 20-state range");
    }

    const auto& state = behavior.states[state_index];
    runtime.current_state = state_index;
    runtime.state_entry_tick = current_tick;
    runtime.timer_delay = choose_inclusive_integer(state.timer_min, state.timer_max, random_value);

    auto& entries = runtime.state_entry_counts[state_index];
    ++entries;

    StateEntryResult result;
    if (state.counter > 0 && entries == state.counter) {
        result.counter_threshold_reached = true;
        result.counter_action = state.on_counter;
    }
    return result;
}

bool state_timer_due(const UnitStateRuntime& runtime, std::uint32_t current_tick) {
    // The PPC stores the delay as a signed int but adds it to the current tick
    // with ordinary 32-bit integer arithmetic.  Cast through uint32_t to model
    // the machine's wraparound rather than C++ signed overflow.
    const auto target = runtime.state_entry_tick + static_cast<std::uint32_t>(runtime.timer_delay);
    return current_tick == target;
}

bool state_range_transition_due(float configured_range, float measured_range) {
    return configured_range != 0.0f && measured_range < configured_range;
}

bool counter_action_resets_entry_count(const ResolvedStateAction& action) {
    // At PPC 0x14e34..0x14e88 a non-empty action other than the sentinel
    // "No State" reaches the state-action path after zeroing the slot.  Our
    // resolver represents empty/No State as none.  Delete/Destroy return
    // earlier and do not need a reset.
    return action.kind == StateActionKind::change_state ||
           action.kind == StateActionKind::unresolved;
}

void reset_current_state_entry_count(UnitStateRuntime& runtime) {
    if (runtime.current_state >= kMaxUnitStates) {
        throw std::out_of_range("current unit state outside original 20-state range");
    }
    runtime.state_entry_counts[runtime.current_state] = 0;
}

} // namespace deimos
