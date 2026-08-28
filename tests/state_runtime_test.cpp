#include "deimos/state_runtime.hpp"

#include <cassert>
#include <cstdint>
#include <stdexcept>

int main() {
    // 0x553E0 exact 32-bit LCG / upper-15-bit sequence.
    deimos::LegacyRandom rng(1);
    assert(rng.next15() == 16838u);
    assert(rng.next15() == 5758u);
    assert(rng.next15() == 10113u);
    assert(rng.next15() == 17515u);
    assert(rng.next15() == 31051u);

    // Equal-range helper bypasses RNG in the original.
    deimos::LegacyRandom equal_rng(1);
    assert(deimos::choose_inclusive_integer(7, 7, equal_rng) == 7);
    assert(equal_rng.seed() == 1u);
    assert(deimos::choose_inclusive_integer(10, 12, equal_rng) ==
           10 + static_cast<int>(16838u % 3u));

    // 0x46580: equal endpoints bypass RNG; otherwise min + rng % width.
    assert(deimos::choose_inclusive_integer(7, 7, 12345) == 7);
    assert(deimos::choose_inclusive_integer(10, 12, 0) == 10);
    assert(deimos::choose_inclusive_integer(10, 12, 1) == 11);
    assert(deimos::choose_inclusive_integer(10, 12, 2) == 12);
    assert(deimos::choose_inclusive_integer(10, 12, 3) == 10);
    bool reversed_threw = false;
    try {
        (void)deimos::choose_inclusive_integer(5, 4, 0);
    } catch (const std::invalid_argument&) {
        reversed_threw = true;
    }
    assert(reversed_threw);

    deimos::CompiledUnitBehavior behavior;
    behavior.states.resize(2);
    behavior.states[0].timer_min = 5;
    behavior.states[0].timer_max = 7;
    behavior.states[0].counter = 2;
    behavior.states[0].on_counter.kind = deimos::StateActionKind::change_state;
    behavior.states[0].on_counter.state_index = 1;
    behavior.states[0].on_counter.original_label = "Next";

    deimos::UnitStateRuntime runtime;
    auto first = deimos::enter_unit_state(runtime, behavior, 0, 100, 1);
    assert(runtime.current_state == 0);
    assert(runtime.state_entry_tick == 100);
    assert(runtime.timer_delay == 6);
    assert(runtime.state_entry_counts[0] == 1);
    assert(!first.counter_threshold_reached);

    // Original timer comparison is exact equality, not >=.
    assert(!deimos::state_timer_due(runtime, 105));
    assert(deimos::state_timer_due(runtime, 106));
    assert(!deimos::state_timer_due(runtime, 107));

    auto second = deimos::enter_unit_state(runtime, behavior, 0, 200, 2);
    assert(runtime.timer_delay == 7);
    assert(runtime.state_entry_counts[0] == 2);
    assert(second.counter_threshold_reached);
    assert(second.counter_action.kind == deimos::StateActionKind::change_state);
    assert(deimos::counter_action_resets_entry_count(second.counter_action));
    deimos::reset_current_state_entry_count(runtime);
    assert(runtime.state_entry_counts[0] == 0);

    deimos::ResolvedStateAction unresolved;
    unresolved.kind = deimos::StateActionKind::unresolved;
    unresolved.original_label = "Stale State Label";
    assert(deimos::counter_action_resets_entry_count(unresolved));
    assert(!deimos::counter_action_resets_entry_count({}));

    deimos::ResolvedStateAction destroy;
    destroy.kind = deimos::StateActionKind::destroy_entity;
    assert(!deimos::counter_action_resets_entry_count(destroy));

    // Range transition is disabled only by exact 0.0 and uses strict <.
    assert(!deimos::state_range_transition_due(0.0f, -1.0f));
    assert(deimos::state_range_transition_due(100.0f, 99.999f));
    assert(!deimos::state_range_transition_due(100.0f, 100.0f));
    assert(!deimos::state_range_transition_due(100.0f, 101.0f));

    bool state_oob_threw = false;
    try {
        (void)deimos::enter_unit_state(runtime, behavior, 20, 0, 0);
    } catch (const std::out_of_range&) {
        state_oob_threw = true;
    }
    assert(state_oob_threw);

    // 32-bit timer arithmetic wraps like the PPC register operation.
    runtime.state_entry_tick = UINT32_MAX - 2u;
    runtime.timer_delay = 5;
    assert(deimos::state_timer_due(runtime, 2u));

    return 0;
}
