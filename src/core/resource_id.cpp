#include "deimos/resource_id.hpp"

#include <algorithm>
#include <cctype>

namespace deimos {

std::string FourCC::str() const { return std::string(bytes.begin(), bytes.end()); }

std::uint32_t FourCC::big_endian_value() const {
    return (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[0])) << 24u) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[1])) << 16u) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[2])) << 8u) |
            static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[3]));
}

static std::string lower(std::string_view value) {
    std::string out(value);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

ResourceKind classify_resource_extension(std::string_view extension) {
    const auto e = lower(extension);
    if (e == ".gif") return ResourceKind::image8;
    if (e == ".tga") return ResourceKind::image16;
    if (e == ".ima" || e == ".aif" || e == ".aiff" || e == ".aifc") return ResourceKind::audio;
    if (e == ".leve" || e == ".level") return ResourceKind::level;
    if (e == ".unde") return ResourceKind::unit;
    if (e == ".wede") return ResourceKind::weapon;
    if (e == ".plde") return ResourceKind::player;
    if (e == ".film") return ResourceKind::film;
    if (e == ".idli" || e == ".idlist") return ResourceKind::id_list;
    if (e == ".flli") return ResourceKind::game_list;
    if (e == ".coli") return ResourceKind::color_list;
    if (e == ".tefo") return ResourceKind::terrain_formation;
    if (e == ".stli") return ResourceKind::string_list;
    if (e == ".reli") return ResourceKind::relation_list;
    return ResourceKind::unknown;
}

std::optional<ResourceName> parse_resource_name(std::string_view input) {
    const auto slash = input.find_last_of("/\\");
    const auto filename_view = input.substr(slash == std::string_view::npos ? 0 : slash + 1);
    const auto dot = filename_view.find_last_of('.');
    if (dot == std::string_view::npos) return std::nullopt;

    const auto close = filename_view.rfind(']', dot);
    if (close == std::string_view::npos || close + 1 != dot) return std::nullopt;
    const auto open = filename_view.rfind('[', close);
    if (open == std::string_view::npos || close - open != 5) return std::nullopt;

    ResourceName result;
    result.filename = std::string(filename_view);
    result.extension = std::string(filename_view.substr(dot));
    result.kind = classify_resource_extension(result.extension);
    for (std::size_t i = 0; i < 4; ++i) result.tag.bytes[i] = filename_view[open + 1 + i];

    auto label = filename_view.substr(0, open);
    if (label.size() >= 3 && label.substr(label.size() - 3) == " IA") {
        result.plate = PlateKind::alpha;
        label.remove_suffix(3);
    } else if (label.size() >= 3 && label.substr(label.size() - 3) == " IC") {
        result.plate = PlateKind::color;
        label.remove_suffix(3);
    }
    result.display_name = std::string(label);
    return result;
}

} // namespace deimos
