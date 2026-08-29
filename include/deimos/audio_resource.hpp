#pragma once

#include "deimos/resource_id.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace deimos {

// Container facts recovered from the Mac 1.0.6 Music.pak AIFC/ima4 assets.
// For QuickTime IMA4, the AIFC COMM frame-count field used by these files is
// the number of 64-sample packet groups, not the final decoded PCM frame count.
struct LegacyAifcInfo {
    std::uint16_t channels = 0;
    std::uint32_t packet_groups = 0;
    std::uint16_t sample_size_bits = 0;
    double sample_rate = 0.0;
    FourCC compression{};
    std::size_t sound_data_offset = 0;
    std::size_t sound_data_size = 0;

    [[nodiscard]] std::uint64_t decoded_frame_count() const {
        return compression.str() == "ima4"
            ? static_cast<std::uint64_t>(packet_groups) * 64u
            : static_cast<std::uint64_t>(packet_groups);
    }
};

struct LegacyPcmAudio {
    std::uint16_t channels = 0;
    std::uint32_t sample_rate = 0;
    std::vector<std::int16_t> interleaved_samples;

    [[nodiscard]] std::size_t frame_count() const {
        return channels == 0 ? 0 : interleaved_samples.size() / channels;
    }
};

// Parses FORM/AIFC, COMM and SSND without relying on platform audio APIs.
[[nodiscard]] std::optional<LegacyAifcInfo> parse_legacy_aifc(
    std::span<const std::uint8_t> bytes,
    std::string* error = nullptr);

// Decodes Apple/QuickTime IMA4 packets exactly as the legacy packet format
// requires: 34 bytes per channel for 64 samples, low nibble first, with the
// 9-bit packet predictor and 7-bit step index header. Predictor continuity is
// retained when a new packet header differs by <= 0x7f and the step index is
// unchanged, matching the QuickTime IMA4 state rule.
[[nodiscard]] std::optional<LegacyPcmAudio> decode_legacy_ima4_aifc(
    std::span<const std::uint8_t> bytes,
    std::string* error = nullptr);

} // namespace deimos
