#include "deimos/resource_store.hpp"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {
void le16(std::vector<std::uint8_t>& b, std::uint16_t v) { b.push_back(v); b.push_back(v >> 8u); }
void le32(std::vector<std::uint8_t>& b, std::uint32_t v) { for (int i=0;i<4;++i) b.push_back(v >> (8u*i)); }
std::uint32_t crc32(const std::vector<std::uint8_t>& bytes) {
    std::uint32_t crc=0xffffffffu; for(auto byte:bytes){crc^=byte;for(int i=0;i<8;++i)crc=(crc>>1u)^(0xedb88320u&(0u-(crc&1u)));} return crc^0xffffffffu;
}
std::vector<std::uint8_t> zip(const std::string& n, const std::vector<std::uint8_t>& p) {
    std::vector<std::uint8_t> o; auto c=crc32(p); auto s=static_cast<std::uint32_t>(p.size());
    le32(o,0x04034b50);le16(o,20);le16(o,0);le16(o,0);le16(o,0);le16(o,0);le32(o,c);le32(o,s);le32(o,s);le16(o,n.size());le16(o,0);o.insert(o.end(),n.begin(),n.end());o.insert(o.end(),p.begin(),p.end());
    auto co=static_cast<std::uint32_t>(o.size());le32(o,0x02014b50);le16(o,20);le16(o,20);le16(o,0);le16(o,0);le16(o,0);le16(o,0);le32(o,c);le32(o,s);le32(o,s);le16(o,n.size());le16(o,0);le16(o,0);le16(o,0);le16(o,0);le32(o,0);le32(o,0);o.insert(o.end(),n.begin(),n.end());auto cs=static_cast<std::uint32_t>(o.size())-co;
    le32(o,0x06054b50);le16(o,0);le16(o,0);le16(o,1);le16(o,1);le32(o,cs);le32(o,co);le16(o,0);return o;
}
}

int main() {
    using namespace deimos;
    std::string error;
    auto pak = PakArchive::parse(zip("im08/Test[test].gif", {1,2,3}), &error);
    assert(pak);
    ResourceStore store; store.add_pak(std::move(*pak));
    auto packaged = store.read("im08/Test[test].gif", &error);
    assert(packaged && !packaged->from_local_override && packaged->bytes == std::vector<std::uint8_t>({1,2,3}));

    const auto root = std::filesystem::temp_directory_path() / "deimos-resource-store-test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "im08");
    { std::ofstream f(root / "im08/Test[test].gif", std::ios::binary); const char x[] = {9,8,7}; f.write(x,3); }
    store.set_local_root(root);
    auto local = store.read("im08/Test[test].gif", &error);
    assert(local && local->from_local_override && local->bytes == std::vector<std::uint8_t>({9,8,7}));
    assert(!store.read("../escape", &error));
    std::filesystem::remove_all(root);
    return 0;
}
