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

// PPC 0x42AD0 -> 0x43090 integer-point angle helper. Startup 0x42920
// generates a 1024-entry atan table from atan(i * 0.01) * 57.2957795 and
// stores truncated integral degrees. This helper reproduces the table lookup
// and original quadrant transform; it is intentionally named neutrally because
// the legacy coordinate/heading convention is not a standard atan2 contract.
[[nodiscard]] int legacy_angle_between_integer_points(
    int x1, int y1, int x2, int y2);

} // namespace deimos
