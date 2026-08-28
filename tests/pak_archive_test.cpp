#include "deimos/pak_archive.hpp"

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

namespace {
void le16(std::vector<std::uint8_t>& b, std::uint16_t v) {
    b.push_back(static_cast<std::uint8_t>(v));
    b.push_back(static_cast<std::uint8_t>(v >> 8u));
}
void le32(std::vector<std::uint8_t>& b, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) b.push_back(static_cast<std::uint8_t>(v >> (8u * i)));
}
std::uint32_t crc32(const std::vector<std::uint8_t>& bytes) {
    std::uint32_t crc = 0xffffffffu;
    for (const auto byte : bytes) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1u) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return crc ^ 0xffffffffu;
}
std::vector<std::uint8_t> make_zip(const std::string& name, const std::vector<std::uint8_t>& payload) {
    std::vector<std::uint8_t> out;
    const auto crc = crc32(payload);
    const auto size = static_cast<std::uint32_t>(payload.size());
    const auto local_offset = static_cast<std::uint32_t>(out.size());
    le32(out, 0x04034b50u); le16(out, 20); le16(out, 0); le16(out, 0); le16(out, 0); le16(out, 0);
    le32(out, crc); le32(out, size); le32(out, size); le16(out, name.size()); le16(out, 0);
    out.insert(out.end(), name.begin(), name.end()); out.insert(out.end(), payload.begin(), payload.end());
    const auto cd_offset = static_cast<std::uint32_t>(out.size());
    le32(out, 0x02014b50u); le16(out, 20); le16(out, 20); le16(out, 0); le16(out, 0); le16(out, 0); le16(out, 0);
    le32(out, crc); le32(out, size); le32(out, size); le16(out, name.size()); le16(out, 0); le16(out, 0);
    le16(out, 0); le16(out, 0); le32(out, 0); le32(out, local_offset); out.insert(out.end(), name.begin(), name.end());
    const auto cd_size = static_cast<std::uint32_t>(out.size()) - cd_offset;
    le32(out, 0x06054b50u); le16(out, 0); le16(out, 0); le16(out, 1); le16(out, 1);
    le32(out, cd_size); le32(out, cd_offset); le16(out, 0);
    return out;
}
}

int main() {
    using namespace deimos;
    const std::string path = "leve/Synthetic[test].leve";
    const std::vector<std::uint8_t> payload = {1, 2, 3, 4, 5};
    std::string error;
    const auto pak = PakArchive::parse(make_zip(path, payload), &error);
    assert(pak);
    assert(pak->entries().size() == 1);
    assert(pak->find(path));
    const auto read = pak->read(path, &error);
    assert(read && *read == payload);
    assert(!pak->read("missing", &error));
    return 0;
}
