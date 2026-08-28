#include "deimos/legacy_math.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace deimos {

float legacy_radians_per_degree() {
    return std::bit_cast<float>(std::uint32_t{0x3C8EFA35u});
}

int legacy_angle_between_integer_points(int x1, int y1, int x2, int y2) {
    // 0x42AD0 forms first-minus-second deltas, then 0x43090 uses the atan
    // table generated at 0x42970..0x429BC. Constants are embedded doubles:
    // 0.01 = 0x3F847AE147AE147B
    // 57.2957795 = 0x404CA5DC1A47A9E3
    static const auto atan_degrees = [] {
        std::array<int, 1024> table{};
        const double step = std::bit_cast<double>(std::uint64_t{0x3F847AE147AE147Bull});
        const double scale = std::bit_cast<double>(std::uint64_t{0x404CA5DC1A47A9E3ull});
        for (std::size_t i = 0; i < table.size(); ++i) {
            const double value = std::atan(static_cast<double>(i) * step) * scale;
            table[i] = static_cast<int>(std::trunc(value));
        }
        return table;
    }();

    const double first = static_cast<double>(x1 - x2);
    const double second = static_cast<double>(y1 - y2);
    const double abs_first = first > 0.0 ? first : -first;
    const double abs_second = second > 0.0 ? second : -second;

    if (first == 0.0 && second == 0.0) return 0;

    double ratio;
    if (abs_first < abs_second) ratio = first / second;
    else ratio = second / first;
    if (ratio < 0.0) ratio = -ratio;

    const double index_scale = std::bit_cast<double>(std::uint64_t{0x4059000000000000ull}); // 100.0
    int index = static_cast<int>(std::trunc(ratio * index_scale));
    if (index < 0) index = 0;
    if (index > 1023) index = 1023;

    int angle = atan_degrees[static_cast<std::size_t>(index)];
    if (angle < 0) angle = -angle;
    if (abs_first < abs_second) angle = 90 - angle;
    if (first < 0.0 && second >= 0.0) angle = 180 - angle;
    if (first < 0.0 && second < 0.0) angle += 180;
    if (first >= 0.0 && second < 0.0) angle = -angle;

    angle -= 90;
    if (angle < 0) angle += 360;
    if (angle >= 360) angle -= 360;
    return angle;
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
