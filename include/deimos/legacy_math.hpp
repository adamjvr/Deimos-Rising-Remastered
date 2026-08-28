#pragma once

#include <array>
#include <cstddef>

namespace deimos {

// Deimos Rising 1.0.6 builds these two tables at startup (PPC 0x42920):
//   angle = float(degrees) * bit_cast<float>(0x3C8EFA35)
//   cos[degrees] = frsp(cos(angle))
//   sin[degrees] = frsp(sin(angle))
// The original MathLib implementation is external to the executable.  This
// clean implementation preserves the recovered input constant, table size,
// call order, and single-precision rounding points.
class LegacyTrigTables {
public:
    static constexpr std::size_t kHeadingCount = 360;

    LegacyTrigTables();

    [[nodiscard]] float cosine(int heading_degrees) const;
    [[nodiscard]] float sine(int heading_degrees) const;

    [[nodiscard]] const std::array<float, kHeadingCount>& cosines() const { return cosines_; }
    [[nodiscard]] const std::array<float, kHeadingCount>& sines() const { return sines_; }

private:
    std::array<float, kHeadingCount> cosines_{};
    std::array<float, kHeadingCount> sines_{};
};

// Exact single-precision radians-per-degree constant embedded in the 1.0.6
// PPC executable at 0x100D7318.
[[nodiscard]] float legacy_radians_per_degree();

} // namespace deimos
