#include "deimos/legacy_math.hpp"

#include <bit>
#include <cassert>
#include <cmath>
#include <cstdint>

int main() {
    const auto factor_bits = std::bit_cast<std::uint32_t>(deimos::legacy_radians_per_degree());
    assert(factor_bits == 0x3C8EFA35u);

    const deimos::LegacyTrigTables trig;
    assert(trig.cosine(0) == 1.0f);
    assert(trig.sine(0) == 0.0f);
    assert(trig.cosine(360) == trig.cosine(0));
    assert(trig.sine(360) == trig.sine(0));

    // The recovered table build path is single precision around MathLib.
    assert(std::fabs(trig.cosine(90)) < 1.0e-6f);
    assert(std::fabs(trig.sine(90) - 1.0f) < 1.0e-6f);
    assert(std::fabs(trig.cosine(180) + 1.0f) < 1.0e-6f);
    return 0;
}
