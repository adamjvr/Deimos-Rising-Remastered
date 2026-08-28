#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace deimos {

struct PakEntry {
    std::string path;
    std::uint32_t crc32 = 0;
    std::uint32_t size = 0;
    std::uint32_t local_header_offset = 0;
    std::uint16_t method = 0;
    bool is_directory = false;
};

class PakArchive {
public:
    static std::optional<PakArchive> open(const std::filesystem::path& path, std::string* error = nullptr);
    static std::optional<PakArchive> parse(std::vector<std::uint8_t> bytes, std::string* error = nullptr);

    [[nodiscard]] const std::vector<PakEntry>& entries() const { return entries_; }
    [[nodiscard]] const PakEntry* find(std::string_view path) const;
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> read(const PakEntry& entry, std::string* error = nullptr) const;
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> read(std::string_view path, std::string* error = nullptr) const;

private:
    std::vector<std::uint8_t> bytes_;
    std::vector<PakEntry> entries_;
};

} // namespace deimos
