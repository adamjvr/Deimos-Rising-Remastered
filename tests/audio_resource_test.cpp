#include "deimos/audio_resource.hpp"

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

namespace {
void be16(std::vector<std::uint8_t>& out, std::uint16_t v) {
    out.push_back(static_cast<std::uint8_t>(v >> 8));
    out.push_back(static_cast<std::uint8_t>(v));
}
void be32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>(v >> 24));
    out.push_back(static_cast<std::uint8_t>(v >> 16));
    out.push_back(static_cast<std::uint8_t>(v >> 8));
    out.push_back(static_cast<std::uint8_t>(v));
}
void text(std::vector<std::uint8_t>& out, const char* s, int n) {
    for (int i = 0; i < n; ++i) out.push_back(static_cast<std::uint8_t>(s[i]));
}
std::vector<std::uint8_t> zero_stereo_ima4() {
    std::vector<std::uint8_t> b;
    text(b, "FORM", 4); be32(b, 0); text(b, "AIFC", 4);

    text(b, "COMM", 4); be32(b, 30);
    be16(b, 2);                 // channels
    be32(b, 1);                 // one 64-frame packet group
    be16(b, 16);
    const std::uint8_t rate[10] = {0x40,0x0e,0xac,0x44,0,0,0,0,0,0}; // 44100
    b.insert(b.end(), std::begin(rate), std::end(rate));
    text(b, "ima4", 4);
    b.push_back(7); text(b, "IMA 4:1", 7); // Pascal compression name

    text(b, "SSND", 4); be32(b, 8 + 68);
    be32(b, 0); be32(b, 0);
    b.insert(b.end(), 68, 0);   // two zero IMA4 channel packets

    const auto form_size = static_cast<std::uint32_t>(b.size() - 8);
    b[4] = static_cast<std::uint8_t>(form_size >> 24);
    b[5] = static_cast<std::uint8_t>(form_size >> 16);
    b[6] = static_cast<std::uint8_t>(form_size >> 8);
    b[7] = static_cast<std::uint8_t>(form_size);
    return b;
}
} // namespace

int main() {
    using namespace deimos;
    const auto bytes = zero_stereo_ima4();
    std::string error;

    const auto info = parse_legacy_aifc(bytes, &error);
    assert(info && error.empty());
    assert(info->channels == 2);
    assert(info->packet_groups == 1);
    assert(info->sample_size_bits == 16);
    assert(info->sample_rate == 44100.0);
    assert(info->compression.str() == "ima4");
    assert(info->decoded_frame_count() == 64);
    assert(info->sound_data_size == 68);

    auto underdeclared = bytes;
    auto declared = static_cast<std::uint32_t>(underdeclared.size() - 8 - 12);
    underdeclared[4] = static_cast<std::uint8_t>(declared >> 24);
    underdeclared[5] = static_cast<std::uint8_t>(declared >> 16);
    underdeclared[6] = static_cast<std::uint8_t>(declared >> 8);
    underdeclared[7] = static_cast<std::uint8_t>(declared);
    const auto underdeclared_info = parse_legacy_aifc(underdeclared, &error);
    assert(underdeclared_info && underdeclared_info->sound_data_size == 68);

    const auto pcm = decode_legacy_ima4_aifc(bytes, &error);
    assert(pcm && error.empty());
    assert(pcm->channels == 2);
    assert(pcm->sample_rate == 44100);
    assert(pcm->frame_count() == 64);
    assert(pcm->interleaved_samples.size() == 128);
    for (const auto sample : pcm->interleaved_samples) assert(sample == 0);

    auto corrupt = bytes;
    // Compression FourCC in COMM begins at byte 38 for this synthetic file.
    corrupt[38] = 'N'; corrupt[39] = 'O'; corrupt[40] = 'N'; corrupt[41] = 'E';
    assert(!decode_legacy_ima4_aifc(corrupt, &error));

    return 0;
}
