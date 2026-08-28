#include "deimos/legacy_math.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace deimos {

float legacy_radians_per_degree() {
    return std::bit_cast<float>(std::uint32_t{0x3C8EFA35u});
}

LegacyTrigTables::LegacyTrigTables() {
    const float factor = legacy_radians_per_degree();
    for (std::size_t degree = 0; degree < kHeadingCount; ++degree) {
        // PPC uses a single-precision multiply before passing the resulting
        // value to MathLib cos/sin, then frsp before storing each table entry.
        const float angle = static_cast<float>(static_cast<float>(degree) * factor);
        cosines_[degree] = static_cast<float>(std::cos(static_cast<double>(angle)));
        sines_[degree] = static_cast<float>(std::sin(static_cast<double>(angle)));
    }
}

namespace {

std::size_t checked_heading_index(int heading_degrees) {
    // PPC lookup helpers 0x42EE0 / 0x42F00 special-case exactly 360 -> 0.
    // Other out-of-range values would index outside the original 360-entry
    // table.  Fail closed in clean code instead of introducing UB.
    if (heading_degrees == 360) return 0;
    if (heading_degrees < 0 || heading_degrees >= static_cast<int>(LegacyTrigTables::kHeadingCount)) {
        throw std::out_of_range("legacy heading outside original 0..360 lookup contract");
    }
    return static_cast<std::size_t>(heading_degrees);
}

} // namespace

float LegacyTrigTables::cosine(int heading_degrees) const {
    return cosines_[checked_heading_index(heading_degrees)];
}

float LegacyTrigTables::sine(int heading_degrees) const {
    return sines_[checked_heading_index(heading_degrees)];
}

} // namespace deimos
