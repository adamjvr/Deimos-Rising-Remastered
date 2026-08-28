#include "deimos/resource_store.hpp"

#include <fstream>
#include <iterator>

namespace deimos {
namespace {

void fail(std::string* error, std::string message) {
    if (error) *error = std::move(message);
}

std::optional<std::vector<std::uint8_t>> read_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return std::nullopt;
    return std::vector<std::uint8_t>(
        std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

} // namespace

bool ResourceStore::safe_relative_path(std::string_view logical_path) {
    if (logical_path.empty() || logical_path.front() == '/' || logical_path.front() == '\\') return false;
    std::filesystem::path path{std::string(logical_path)};
    if (path.is_absolute() || path.has_root_name() || path.has_root_directory()) return false;
    for (const auto& part : path) {
        if (part == "..") return false;
    }
    return true;
}

std::optional<ResourceBytes> ResourceStore::read(std::string_view logical_path, std::string* error) const {
    if (!safe_relative_path(logical_path)) {
        fail(error, "unsafe resource path: " + std::string(logical_path));
        return std::nullopt;
    }

    if (local_root_) {
        const auto path = *local_root_ / std::filesystem::path(std::string(logical_path));
        std::error_code ec;
        if (std::filesystem::is_regular_file(path, ec) && !ec) {
            auto bytes = read_file(path);
            if (!bytes) {
                fail(error, "could not read Local override: " + path.string());
                return std::nullopt;
            }
            return ResourceBytes{std::move(*bytes), true, 0};
        }
    }

    for (std::size_t reverse = paks_.size(); reverse > 0; --reverse) {
        const auto index = reverse - 1;
        const auto* entry = paks_[index].find(logical_path);
        if (!entry || entry->is_directory) continue;
        auto bytes = paks_[index].read(*entry, error);
        if (!bytes) return std::nullopt;
        return ResourceBytes{std::move(*bytes), false, index};
    }

    fail(error, "resource not found: " + std::string(logical_path));
    return std::nullopt;
}

} // namespace deimos
