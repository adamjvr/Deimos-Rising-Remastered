#include "deimos/audio_resource.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace deimos {
namespace {

void fail(std::string* error, std::string message) {
    if (error) *error = std::move(message);
}

std::uint16_t be16(std::span<const std::uint8_t> bytes, std::size_t at) {
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(bytes[at]) << 8u) |
        static_cast<std::uint16_t>(bytes[at + 1]));
}

std::uint32_t be32(std::span<const std::uint8_t> bytes, std::size_t at) {
    return (static_cast<std::uint32_t>(bytes[at]) << 24u) |
           (static_cast<std::uint32_t>(bytes[at + 1]) << 16u) |
           (static_cast<std::uint32_t>(bytes[at + 2]) << 8u) |
            static_cast<std::uint32_t>(bytes[at + 3]);
}

std::uint64_t be64(std::span<const std::uint8_t> bytes, std::size_t at) {
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < 8; ++i) value = (value << 8u) | bytes[at + i];
    return value;
}

bool fourcc_at(std::span<const std::uint8_t> bytes, std::size_t at, const char (&text)[5]) {
    return at + 4 <= bytes.size() &&
           bytes[at] == static_cast<std::uint8_t>(text[0]) &&
           bytes[at + 1] == static_cast<std::uint8_t>(text[1]) &&
           bytes[at + 2] == static_cast<std::uint8_t>(text[2]) &&
           bytes[at + 3] == static_cast<std::uint8_t>(text[3]);
}

FourCC read_fourcc(std::span<const std::uint8_t> bytes, std::size_t at) {
    FourCC id{};
    for (std::size_t i = 0; i < 4; ++i) id.bytes[i] = static_cast<char>(bytes[at + i]);
    return id;
}

std::optional<double> extended80(std::span<const std::uint8_t> bytes, std::size_t at) {
    if (at + 10 > bytes.size()) return std::nullopt;
    const auto sign_exp = be16(bytes, at);
    const bool negative = (sign_exp & 0x8000u) != 0;
    const auto exponent = static_cast<unsigned>(sign_exp & 0x7fffu);
    const auto mantissa = be64(bytes, at + 2);
    if (exponent == 0 && mantissa == 0) return 0.0;
    if (exponent == 0x7fffu) return std::nullopt;

    // 80-bit extended: explicit integer bit in a 64-bit significand.
    const int power = static_cast<int>(exponent) - 16383 - 63;
    long double value = std::ldexp(static_cast<long double>(mantissa), power);
    if (negative) value = -value;
    if (!std::isfinite(value) || value <= 0.0L ||
        value > static_cast<long double>(std::numeric_limits<double>::max())) {
        return std::nullopt;
    }
    return static_cast<double>(value);
}

struct ImaState {
    int predictor = 0;
    int step_index = 0;
};

constexpr std::array<int, 89> kStepTable = {
    7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,
    34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,
    157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,
    724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,
    2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,
    10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767
};
constexpr std::array<int, 8> kIndexTable = {-1,-1,-1,-1,2,4,6,8};

std::int16_t expand_ima_nibble(ImaState& state, int nibble) {
    const int step = kStepTable[static_cast<std::size_t>(state.step_index)];
    int diff = step >> 3;
    if ((nibble & 4) != 0) diff += step;
    if ((nibble & 2) != 0) diff += step >> 1;
    if ((nibble & 1) != 0) diff += step >> 2;

    state.predictor += (nibble & 8) != 0 ? -diff : diff;
    state.predictor = std::clamp(state.predictor, -32768, 32767);
    state.step_index = std::clamp(
        state.step_index + kIndexTable[static_cast<std::size_t>(nibble & 7)], 0, 88);
    return static_cast<std::int16_t>(state.predictor);
}

bool decode_packet(
    std::span<const std::uint8_t> packet,
    ImaState& state,
    std::array<std::int16_t, 64>& output,
    std::string* error) {
    if (packet.size() != 34) {
        fail(error, "IMA4 channel packet must be exactly 34 bytes");
        return false;
    }
    const auto raw_header = be16(packet, 0);
    int packet_predictor = static_cast<int>(raw_header & 0xff80u);
    if ((packet_predictor & 0x8000) != 0) packet_predictor -= 0x10000;
    const int packet_index = static_cast<int>(raw_header & 0x007fu);
    if (packet_index > 88) {
        fail(error, "IMA4 packet step index exceeds 88");
        return false;
    }

    // QuickTime IMA4 headers carry only the predictor's top nine bits. Keep
    // the more precise running predictor when the header describes the same
    // state within that 7-bit quantization window.
    if (state.step_index != packet_index || std::abs(packet_predictor - state.predictor) > 0x7f) {
        state.step_index = packet_index;
        state.predictor = packet_predictor;
    }

    std::size_t sample = 0;
    for (std::size_t i = 2; i < packet.size(); ++i) {
        const int byte = packet[i];
        output[sample++] = expand_ima_nibble(state, byte & 0x0f);
        output[sample++] = expand_ima_nibble(state, byte >> 4);
    }
    return true;
}

} // namespace

std::optional<LegacyAifcInfo> parse_legacy_aifc(
    std::span<const std::uint8_t> bytes,
    std::string* error) {
    if (error) error->clear();
    if (bytes.size() < 12 || !fourcc_at(bytes, 0, "FORM") || !fourcc_at(bytes, 8, "AIFC")) {
        fail(error, "resource is not FORM/AIFC");
        return std::nullopt;
    }
    const auto form_size = static_cast<std::uint64_t>(be32(bytes, 4)) + 8u;
    if (form_size > bytes.size()) {
        fail(error, "AIFC FORM size exceeds input");
        return std::nullopt;
    }

    LegacyAifcInfo info{};
    bool have_comm = false;
    bool have_ssnd = false;
    std::size_t cursor = 12;
    // Canonical mu03 under-declares FORM by 76 bytes while carrying a valid
    // SSND chunk to EOF. QuickTime accepts this legacy asset, so treat FORM
    // as a lower-bound/advisory length after ensuring it does not exceed the
    // input and scan complete chunks through the resource payload.
    const auto form_end = bytes.size();
    while (cursor + 8 <= form_end) {
        const auto chunk_size = static_cast<std::size_t>(be32(bytes, cursor + 4));
        const auto data = cursor + 8;
        if (data > form_end || chunk_size > form_end - data) {
            fail(error, "AIFC chunk extends beyond FORM");
            return std::nullopt;
        }

        if (fourcc_at(bytes, cursor, "COMM")) {
            if (chunk_size < 22) {
                fail(error, "AIFC COMM chunk is too short");
                return std::nullopt;
            }
            info.channels = be16(bytes, data);
            info.packet_groups = be32(bytes, data + 2);
            info.sample_size_bits = be16(bytes, data + 6);
            const auto rate = extended80(bytes, data + 8);
            if (!rate) {
                fail(error, "invalid AIFC 80-bit sample rate");
                return std::nullopt;
            }
            info.sample_rate = *rate;
            info.compression = read_fourcc(bytes, data + 18);
            have_comm = true;
        } else if (fourcc_at(bytes, cursor, "SSND")) {
            if (chunk_size < 8) {
                fail(error, "AIFC SSND chunk is too short");
                return std::nullopt;
            }
            const auto offset = static_cast<std::size_t>(be32(bytes, data));
            if (offset > chunk_size - 8) {
                fail(error, "AIFC SSND offset exceeds chunk payload");
                return std::nullopt;
            }
            info.sound_data_offset = data + 8 + offset;
            info.sound_data_size = chunk_size - 8 - offset;
            have_ssnd = true;
        }

        const auto padded = chunk_size + (chunk_size & 1u);
        if (padded > form_end - data) break;
        cursor = data + padded;
    }

    if (!have_comm || !have_ssnd) {
        fail(error, "AIFC is missing COMM or SSND");
        return std::nullopt;
    }
    if (info.channels == 0) {
        fail(error, "AIFC channel count is zero");
        return std::nullopt;
    }
    if (info.sound_data_offset > bytes.size() || info.sound_data_size > bytes.size() - info.sound_data_offset) {
        fail(error, "AIFC sound data lies outside input");
        return std::nullopt;
    }
    return info;
}

std::optional<LegacyPcmAudio> decode_legacy_ima4_aifc(
    std::span<const std::uint8_t> bytes,
    std::string* error) {
    auto info = parse_legacy_aifc(bytes, error);
    if (!info) return std::nullopt;
    if (info->compression.str() != "ima4") {
        fail(error, "AIFC compression is not ima4");
        return std::nullopt;
    }
    if (info->sample_size_bits != 16) {
        fail(error, "legacy ima4 resource is not 16-bit PCM");
        return std::nullopt;
    }

    constexpr std::size_t packet_bytes = 34;
    constexpr std::size_t packet_samples = 64;
    const auto bytes_per_group = packet_bytes * static_cast<std::size_t>(info->channels);
    if (info->packet_groups > 0 && bytes_per_group > std::numeric_limits<std::size_t>::max() / info->packet_groups) {
        fail(error, "IMA4 packet count overflows host size");
        return std::nullopt;
    }
    const auto required_bytes = bytes_per_group * static_cast<std::size_t>(info->packet_groups);
    if (required_bytes > info->sound_data_size) {
        fail(error, "IMA4 SSND payload is shorter than COMM packet count");
        return std::nullopt;
    }

    const auto rounded_rate = std::llround(info->sample_rate);
    if (rounded_rate <= 0 || rounded_rate > std::numeric_limits<std::uint32_t>::max() ||
        std::abs(info->sample_rate - static_cast<double>(rounded_rate)) > 0.001) {
        fail(error, "IMA4 sample rate is not an integral supported rate");
        return std::nullopt;
    }

    const auto frames64 = static_cast<std::uint64_t>(info->packet_groups) * packet_samples;
    const auto samples64 = frames64 * info->channels;
    if (samples64 > std::numeric_limits<std::size_t>::max()) {
        fail(error, "IMA4 decoded sample count overflows host size");
        return std::nullopt;
    }

    LegacyPcmAudio pcm;
    pcm.channels = info->channels;
    pcm.sample_rate = static_cast<std::uint32_t>(rounded_rate);
    pcm.interleaved_samples.reserve(static_cast<std::size_t>(samples64));

    std::vector<ImaState> states(info->channels);
    std::vector<std::array<std::int16_t, packet_samples>> channel_samples(info->channels);
    std::size_t cursor = info->sound_data_offset;
    for (std::uint32_t group = 0; group < info->packet_groups; ++group) {
        for (std::uint16_t channel = 0; channel < info->channels; ++channel) {
            const std::span<const std::uint8_t> packet(bytes.data() + cursor, packet_bytes);
            if (!decode_packet(packet, states[channel], channel_samples[channel], error)) return std::nullopt;
            cursor += packet_bytes;
        }
        for (std::size_t sample = 0; sample < packet_samples; ++sample) {
            for (std::uint16_t channel = 0; channel < info->channels; ++channel) {
                pcm.interleaved_samples.push_back(channel_samples[channel][sample]);
            }
        }
    }
    return pcm;
}

} // namespace deimos
