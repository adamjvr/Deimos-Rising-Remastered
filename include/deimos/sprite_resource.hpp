#pragma once

#include "deimos/resource_id.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace deimos {

// Indexed 8-bit image used by the Mac sprite-plate scanner at 0x1F140.
// GIF palette RGB values are irrelevant to that algorithm: it compares the
// decoded palette indices byte-for-byte.
struct LegacyIndexedImage {
    int width = 0;
    int height = 0;
    int row_bytes = 0;
    std::vector<std::uint8_t> pixels;
};

struct LegacySpriteRect {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;

    [[nodiscard]] int width() const { return right - left; }
    [[nodiscard]] int height() const { return bottom - top; }
    constexpr bool operator==(const LegacySpriteRect&) const = default;
};

struct LegacySpriteFrameMetadata {
    LegacySpriteRect source_rect{};
    int width = 0;
    int height = 0;
};

struct LegacySpriteGroupMetadata {
    FourCC id{};
    std::vector<LegacySpriteFrameMetadata> frames;
};

// Minimal GIF87a/89a decoder sufficient for the original im08 plates. It
// returns decoded palette indices, preserving the exact byte domain consumed
// by 0x1F140..0x1F5B0. The first image is composited into the logical-screen
// indexed canvas; extensions are skipped and interlace is supported.
[[nodiscard]] std::optional<LegacyIndexedImage> decode_legacy_gif_indices(
    std::span<const std::uint8_t> bytes,
    std::string* error = nullptr);

// Exact alpha-plate rectangle scanner reconstructed from 0x1F140/0x1F1C0,
// 0x1F340, 0x1F4E0, 0x1F540 and 0x1F5B0. The alpha plate's second decoded
// byte is the separator marker. Cells are found in scan order and trimmed by
// their own top-left byte value.
[[nodiscard]] std::optional<std::vector<LegacySpriteFrameMetadata>>
extract_legacy_sprite_frames(
    const LegacyIndexedImage& alpha_plate,
    std::string* error = nullptr);

// 0x19C10 scaled-dimension semantics: exact scale 1 returns stored dimensions;
// otherwise each axis is multiplied and truncated toward zero (PPC fctiwz).
[[nodiscard]] std::pair<int, int> legacy_scaled_sprite_dimensions(
    const LegacySpriteFrameMetadata& frame,
    float scale);

class LegacySpriteCache {
public:
    using Loader = std::function<bool(FourCC, LegacySpriteCache&)>;

    // Publishes a complete group atomically. This mirrors the loader contract:
    // consumers never observe a partially constructed frame list.
    bool publish(LegacySpriteGroupMetadata group);

    [[nodiscard]] const LegacySpriteFrameMetadata* find_loaded_frame(FourCC id, int frame) const;
    [[nodiscard]] std::size_t frame_count(FourCC id) const;

    // 0x19CA0 behavior: 'none' -> 0,0; lookup first; if absent, invoke the
    // loader once for the group and retry. A requested frame >= frame count is
    // normalized to frame zero by find_loaded_frame, matching 0x19AD0.
    [[nodiscard]] std::pair<int, int> dimensions(
        FourCC id,
        int frame,
        float scale,
        const Loader& loader = {});

    [[nodiscard]] std::size_t group_count() const { return groups_.size(); }

private:
    std::vector<LegacySpriteGroupMetadata> groups_;
};

} // namespace deimos
