#include "deimos/state_runtime.hpp"

#include <stdexcept>

namespace deimos {

std::uint32_t LegacyRandom::next15() {
    seed_ = seed_ * 1103515245u + 12345u;
    return (seed_ >> 16u) & 0x7FFFu;
}

int choose_inclusive_integer(int minimum, int maximum, std::uint32_t random_value) {
    if (minimum == maximum) return minimum;
    if (maximum < minimum) {
        throw std::invalid_argument("inclusive random range has maximum < minimum");
    }
    const auto width = static_cast<std::uint32_t>(maximum - minimum) + 1u;
    return minimum + static_cast<int>(random_value % width);
}

int choose_inclusive_integer(int minimum, int maximum, LegacyRandom& random) {
    if (minimum == maximum) return minimum; // original does not consume RNG here
    return choose_inclusive_integer(minimum, maximum, random.next15());
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
