#include "deimos/pak_archive.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>

namespace deimos {
namespace {

std::uint16_t le16(std::span<const std::uint8_t> b, std::size_t o) {
    return static_cast<std::uint16_t>(b[o]) | (static_cast<std::uint16_t>(b[o + 1]) << 8u);
}
std::uint32_t le32(std::span<const std::uint8_t> b, std::size_t o) {
    return static_cast<std::uint32_t>(b[o]) |
           (static_cast<std::uint32_t>(b[o + 1]) << 8u) |
           (static_cast<std::uint32_t>(b[o + 2]) << 16u) |
           (static_cast<std::uint32_t>(b[o + 3]) << 24u);
}
void fail(std::string* error, std::string message) { if (error) *error = std::move(message); }

std::uint32_t crc32(std::span<const std::uint8_t> bytes) {
    std::uint32_t crc = 0xffffffffu;
    for (const auto byte : bytes) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1u) ^ (0xedb88320u & (0u - (crc & 1u)));
        }
    }
    return crc ^ 0xffffffffu;
}

} // namespace

std::optional<PakArchive> PakArchive::open(const std::filesystem::path& path, std::string* error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        fail(error, "could not open PAK: " + path.string());
        return std::nullopt;
    }
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return parse(std::move(bytes), error);
}

std::optional<PakArchive> PakArchive::parse(std::vector<std::uint8_t> bytes, std::string* error) {
    // Original 1.0.6 PAKs are conventional ZIP files whose members are all
    // method 0 (stored). Locate End Of Central Directory within ZIP's legal
    // 64K comment search window.
    if (bytes.size() < 22) {
        fail(error, "PAK is too small to contain ZIP EOCD");
        return std::nullopt;
    }
    const std::size_t search_begin = bytes.size() > (0xffffu + 22u) ? bytes.size() - (0xffffu + 22u) : 0;
    std::optional<std::size_t> eocd;
    for (std::size_t pos = bytes.size() - 22u;; --pos) {
        if (le32(bytes, pos) == 0x06054b50u) { eocd = pos; break; }
        if (pos == search_begin) break;
    }
    if (!eocd) {
        fail(error, "ZIP EOCD signature not found");
        return std::nullopt;
    }

    const auto disk = le16(bytes, *eocd + 4);
    const auto cd_disk = le16(bytes, *eocd + 6);
    const auto entries_on_disk = le16(bytes, *eocd + 8);
    const auto entry_count = le16(bytes, *eocd + 10);
    const auto cd_size = le32(bytes, *eocd + 12);
    const auto cd_offset = le32(bytes, *eocd + 16);
    if (disk != 0 || cd_disk != 0 || entries_on_disk != entry_count) {
        fail(error, "multi-disk ZIP PAKs are unsupported");
        return std::nullopt;
    }
    if (static_cast<std::uint64_t>(cd_offset) + cd_size > bytes.size()) {
        fail(error, "central directory lies outside PAK");
        return std::nullopt;
    }

    PakArchive pak;
    pak.bytes_ = std::move(bytes);
    pak.entries_.reserve(entry_count);
    std::size_t cursor = cd_offset;
    for (std::uint16_t i = 0; i < entry_count; ++i) {
        if (cursor + 46 > pak.bytes_.size() || le32(pak.bytes_, cursor) != 0x02014b50u) {
            fail(error, "invalid ZIP central-directory entry");
            return std::nullopt;
        }
        const auto flags = le16(pak.bytes_, cursor + 8);
        const auto method = le16(pak.bytes_, cursor + 10);
        const auto crc = le32(pak.bytes_, cursor + 16);
        const auto compressed_size = le32(pak.bytes_, cursor + 20);
        const auto size = le32(pak.bytes_, cursor + 24);
        const auto name_len = le16(pak.bytes_, cursor + 28);
        const auto extra_len = le16(pak.bytes_, cursor + 30);
        const auto comment_len = le16(pak.bytes_, cursor + 32);
        const auto local_offset = le32(pak.bytes_, cursor + 42);
        const auto end = cursor + 46u + name_len + extra_len + comment_len;
        if (end > pak.bytes_.size()) {
            fail(error, "truncated ZIP central-directory entry");
            return std::nullopt;
        }
        if ((flags & 0x0001u) != 0) {
            fail(error, "encrypted ZIP member is unsupported");
            return std::nullopt;
        }
        if (method != 0 || compressed_size != size) {
            fail(error, "compressed ZIP member encountered; canonical Deimos PAKs are stored-only");
            return std::nullopt;
        }
        std::string name(reinterpret_cast<const char*>(pak.bytes_.data() + cursor + 46), name_len);
        pak.entries_.push_back({name, crc, size, local_offset, method, !name.empty() && name.back() == '/'});
        cursor = end;
    }
    return pak;
}

const PakEntry* PakArchive::find(std::string_view path) const {
    const auto it = std::find_if(entries_.begin(), entries_.end(), [path](const PakEntry& entry) {
        return entry.path == path;
    });
    return it == entries_.end() ? nullptr : &*it;
}

std::optional<std::vector<std::uint8_t>> PakArchive::read(const PakEntry& entry, std::string* error) const {
    const auto offset = static_cast<std::size_t>(entry.local_header_offset);
    if (offset + 30 > bytes_.size() || le32(bytes_, offset) != 0x04034b50u) {
        fail(error, "invalid local ZIP header for " + entry.path);
        return std::nullopt;
    }
    const auto flags = le16(bytes_, offset + 6);
    const auto method = le16(bytes_, offset + 8);
    const auto name_len = le16(bytes_, offset + 26);
    const auto extra_len = le16(bytes_, offset + 28);
    if ((flags & 0x0001u) != 0 || method != 0 || method != entry.method) {
        fail(error, "unsupported or inconsistent local ZIP header for " + entry.path);
        return std::nullopt;
    }
    const auto name_offset = offset + 30u;
    const auto data_offset = name_offset + name_len + extra_len;
    if (data_offset > bytes_.size() ||
        static_cast<std::uint64_t>(data_offset) + entry.size > bytes_.size()) {
        fail(error, "PAK entry extends beyond archive: " + entry.path);
        return std::nullopt;
    }
    const std::string local_name(reinterpret_cast<const char*>(bytes_.data() + name_offset), name_len);
    if (local_name != entry.path) {
        fail(error, "central/local ZIP filename mismatch for " + entry.path);
        return std::nullopt;
    }
    std::vector<std::uint8_t> result(
        bytes_.begin() + static_cast<std::ptrdiff_t>(data_offset),
        bytes_.begin() + static_cast<std::ptrdiff_t>(data_offset + entry.size));
    if (crc32(result) != entry.crc32) {
        fail(error, "CRC32 mismatch for PAK entry: " + entry.path);
        return std::nullopt;
    }
    return result;
}

std::optional<std::vector<std::uint8_t>> PakArchive::read(std::string_view path, std::string* error) const {
    const auto* entry = find(path);
    if (!entry) {
        fail(error, "PAK entry not found: " + std::string(path));
        return std::nullopt;
    }
    return read(*entry, error);
}

} // namespace deimos
