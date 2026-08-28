#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace deimos {

enum class PlateKind { none, alpha, color };

enum class ResourceKind {
    unknown,
    image8,
    image16,
    audio,
    level,
    unit,
    weapon,
    player,
    film,
    id_list,
    float_list,
    color_list,
    text_format,
    string_list,
    rect_list
};

struct FourCC {
    std::array<char, 4> bytes{};
    constexpr bool operator==(const FourCC&) const = default;
    std::string str() const;
    std::uint32_t big_endian_value() const;
};

struct ResourceName {
    std::string filename;
    std::string display_name;
    std::string extension;
    FourCC tag{};
    PlateKind plate = PlateKind::none;
    ResourceKind kind = ResourceKind::unknown;
};

std::optional<ResourceName> parse_resource_name(std::string_view filename);
ResourceKind classify_resource_extension(std::string_view extension);

} // namespace deimos
