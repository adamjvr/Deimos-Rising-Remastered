#include "deimos/sprite_resource.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <string_view>

namespace deimos {
namespace {

constexpr FourCC fourcc(char a, char b, char c, char d) {
    return FourCC{{a, b, c, d}};
}

void fail(std::string* error, std::string message) {
    if (error) *error = std::move(message);
}

std::uint16_t le16(std::span<const std::uint8_t> b, std::size_t o) {
    return static_cast<std::uint16_t>(b[o]) |
           (static_cast<std::uint16_t>(b[o + 1]) << 8u);
}

std::uint16_t pack_rgb555(std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    // Classic 16-bit QuickDraw direct pixels use xRGB1555. The GIF importer
    // truncates each 8-bit palette component to its high five bits.
    return static_cast<std::uint16_t>(((r >> 3u) << 10u) | ((g >> 3u) << 5u) | (b >> 3u));
}

std::optional<std::vector<std::uint16_t>> read_palette555(
    std::span<const std::uint8_t> bytes,
    std::size_t& pos,
    std::size_t entries,
    std::string* error) {
    if (entries == 0 || entries > 256u || entries * 3u > bytes.size() - pos) {
        fail(error, "truncated/invalid GIF color table");
        return std::nullopt;
    }
    std::vector<std::uint16_t> palette;
    palette.reserve(entries);
    for (std::size_t i = 0; i < entries; ++i) {
        palette.push_back(pack_rgb555(bytes[pos], bytes[pos + 1u], bytes[pos + 2u]));
        pos += 3u;
    }
    return palette;
}

bool skip_sub_blocks(std::span<const std::uint8_t> bytes, std::size_t& pos) {
    for (;;) {
        if (pos >= bytes.size()) return false;
        const auto n = static_cast<std::size_t>(bytes[pos++]);
        if (n == 0) return true;
        if (n > bytes.size() - pos) return false;
        pos += n;
    }
}

std::optional<std::vector<std::uint8_t>> read_sub_blocks(
    std::span<const std::uint8_t> bytes,
    std::size_t& pos) {
    std::vector<std::uint8_t> out;
    for (;;) {
        if (pos >= bytes.size()) return std::nullopt;
        const auto n = static_cast<std::size_t>(bytes[pos++]);
        if (n == 0) return out;
        if (n > bytes.size() - pos) return std::nullopt;
        out.insert(out.end(), bytes.begin() + static_cast<std::ptrdiff_t>(pos),
                   bytes.begin() + static_cast<std::ptrdiff_t>(pos + n));
        pos += n;
    }
}

std::optional<std::vector<std::uint8_t>> gif_lzw_decode(
    std::span<const std::uint8_t> compressed,
    int minimum_code_size,
    std::size_t expected,
    std::string* error) {
    if (minimum_code_size < 2 || minimum_code_size > 8) {
        fail(error, "unsupported GIF LZW minimum code size");
        return std::nullopt;
    }

    const int clear_code = 1 << minimum_code_size;
    const int end_code = clear_code + 1;
    int code_size = minimum_code_size + 1;

    std::vector<std::vector<std::uint8_t>> dict;
    dict.reserve(4096);
    auto reset = [&]() {
        dict.clear();
        for (int i = 0; i < clear_code; ++i) dict.push_back({static_cast<std::uint8_t>(i)});
        dict.push_back({}); // clear
        dict.push_back({}); // end
        code_size = minimum_code_size + 1;
    };
    reset();

    std::size_t bit_pos = 0;
    auto read_code = [&]() -> std::optional<int> {
        if (bit_pos + static_cast<std::size_t>(code_size) > compressed.size() * 8u) return std::nullopt;
        int code = 0;
        for (int bit = 0; bit < code_size; ++bit) {
            const auto absolute = bit_pos + static_cast<std::size_t>(bit);
            const int value = (compressed[absolute >> 3u] >> (absolute & 7u)) & 1u;
            code |= value << bit;
        }
        bit_pos += static_cast<std::size_t>(code_size);
        return code;
    };

    std::vector<std::uint8_t> output;
    output.reserve(expected);
    std::vector<std::uint8_t> previous;

    while (auto maybe_code = read_code()) {
        const int code = *maybe_code;
        if (code == clear_code) {
            reset();
            previous.clear();
            continue;
        }
        if (code == end_code) break;

        std::vector<std::uint8_t> entry;
        if (code >= 0 && static_cast<std::size_t>(code) < dict.size() && !dict[code].empty()) {
            entry = dict[code];
        } else if (static_cast<std::size_t>(code) == dict.size() && !previous.empty()) {
            entry = previous;
            entry.push_back(previous.front());
        } else {
            fail(error, "invalid GIF LZW code");
            return std::nullopt;
        }

        if (output.size() + entry.size() > expected + 4096u) {
            fail(error, "GIF LZW output exceeds image bounds");
            return std::nullopt;
        }
        output.insert(output.end(), entry.begin(), entry.end());

        if (!previous.empty() && dict.size() < 4096u) {
            auto next = previous;
            next.push_back(entry.front());
            dict.push_back(std::move(next));
            if (static_cast<int>(dict.size()) == (1 << code_size) && code_size < 12) ++code_size;
        }
        previous = std::move(entry);
        if (output.size() >= expected) break;
    }

    if (output.size() < expected) {
        fail(error, "truncated GIF LZW image data");
        return std::nullopt;
    }
    output.resize(expected);
    return output;
}

bool valid_image(const LegacyIndexedImage& image) {
    return image.width > 0 && image.height > 0 && image.row_bytes >= image.width &&
           static_cast<std::uint64_t>(image.row_bytes) * static_cast<std::uint64_t>(image.height) <= image.pixels.size();
}

// 0x1F4E0: find the positive distance from start_y to the next separator
// marker in the first column.
std::optional<int> marker_row_distance(const LegacyIndexedImage& image, int start_y) {
    const auto marker = image.pixels[1];
    int distance = 0;
    std::size_t offset = static_cast<std::size_t>(start_y) * image.row_bytes;
    while (start_y + distance < image.height && image.pixels[offset] != marker) {
        offset += static_cast<std::size_t>(image.row_bytes);
        ++distance;
    }
    if (distance <= 0) return std::nullopt;
    return distance;
}

// 0x1F540: within one marker-bounded row band, find the positive horizontal
// distance to the next column made entirely from the separator marker.
std::optional<int> marker_column_distance(
    const LegacyIndexedImage& image,
    int start_y,
    int band_height,
    int start_x) {
    const auto row_base = static_cast<std::size_t>(start_y) * image.row_bytes;
    const auto marker = image.pixels[row_base + 1u];
    int distance = 0;
    while (start_x + distance < image.width) {
        bool full_marker_column = true;
        for (int y = 0; y < band_height; ++y) {
            const auto offset = row_base + static_cast<std::size_t>(y) * image.row_bytes +
                                static_cast<std::size_t>(start_x + distance);
            if (image.pixels[offset] != marker) {
                full_marker_column = false;
                break;
            }
        }
        if (full_marker_column) break;
        ++distance;
    }
    if (distance <= 0) return std::nullopt;
    return distance;
}

struct TrimmedCell {
    int width = 0;
    int height = 0;
};

// 0x1F5B0: trim complete rows/columns matching the candidate cell's own
// upper-left value. Only the dimensions are needed by the frame record.
std::optional<TrimmedCell> trim_marker_cell(
    const LegacyIndexedImage& image,
    int x,
    int y,
    int width,
    int height) {
    const auto at = [&](int px, int py) -> std::uint8_t {
        return image.pixels[static_cast<std::size_t>(py) * image.row_bytes + static_cast<std::size_t>(px)];
    };
    const auto marker = at(x, y);

    int top = 0;
    while (top < height) {
        bool full = true;
        for (int xx = 0; xx < width; ++xx) {
            if (at(x + xx, y + top) != marker) { full = false; break; }
        }
        if (!full) break;
        ++top;
    }
    if (top >= height) return std::nullopt;

    int bottom = 0;
    while (bottom < height) {
        bool full = true;
        for (int xx = 0; xx < width; ++xx) {
            if (at(x + xx, y + height - bottom - 1) != marker) { full = false; break; }
        }
        if (!full) break;
        ++bottom;
    }
    const int kept_height = height - top - bottom;

    int left = 0;
    while (left < width) {
        bool full = true;
        for (int yy = 0; yy < kept_height; ++yy) {
            if (at(x + left, y + top + yy) != marker) { full = false; break; }
        }
        if (!full) break;
        ++left;
    }

    int right = 0;
    while (right < width) {
        bool full = true;
        for (int yy = 0; yy < kept_height; ++yy) {
            if (at(x + width - right - 1, y + top + yy) != marker) { full = false; break; }
        }
        if (!full) break;
        ++right;
    }

    return TrimmedCell{width - left - right, kept_height};
}

} // namespace

std::optional<LegacyIndexedImage> decode_legacy_gif_indices(
    std::span<const std::uint8_t> bytes,
    std::string* error) {
    if (bytes.size() < 13u) {
        fail(error, "GIF is too small");
        return std::nullopt;
    }
    const std::string_view signature(reinterpret_cast<const char*>(bytes.data()), 6);
    if (signature != "GIF87a" && signature != "GIF89a") {
        fail(error, "invalid GIF signature");
        return std::nullopt;
    }

    const int screen_width = le16(bytes, 6);
    const int screen_height = le16(bytes, 8);
    if (screen_width <= 0 || screen_height <= 0) {
        fail(error, "invalid GIF logical-screen dimensions");
        return std::nullopt;
    }
    const std::uint64_t canvas_size = static_cast<std::uint64_t>(screen_width) * screen_height;
    if (canvas_size > 256u * 1024u * 1024u) {
        fail(error, "GIF logical screen is unreasonably large");
        return std::nullopt;
    }

    std::size_t pos = 13;
    const auto packed = bytes[10];
    std::vector<std::uint16_t> global_palette;
    if ((packed & 0x80u) != 0) {
        const std::size_t table_entries = 1u << ((packed & 0x07u) + 1u);
        auto palette = read_palette555(bytes, pos, table_entries, error);
        if (!palette) return std::nullopt;
        global_palette = std::move(*palette);
    }

    const auto background_index = bytes[11];
    const std::uint16_t background_rgb555 =
        background_index < global_palette.size() ? global_palette[background_index] : 0u;
    std::vector<std::uint8_t> canvas(static_cast<std::size_t>(canvas_size), background_index);
    std::vector<std::uint16_t> rgb555_canvas(static_cast<std::size_t>(canvas_size), background_rgb555);
    bool decoded_image = false;

    while (pos < bytes.size()) {
        const auto introducer = bytes[pos++];
        if (introducer == 0x3bu) break; // trailer
        if (introducer == 0x21u) {
            if (pos >= bytes.size()) { fail(error, "truncated GIF extension"); return std::nullopt; }
            ++pos; // extension label
            if (!skip_sub_blocks(bytes, pos)) { fail(error, "truncated GIF extension blocks"); return std::nullopt; }
            continue;
        }
        if (introducer != 0x2cu) {
            fail(error, "unknown GIF block introducer");
            return std::nullopt;
        }
        if (pos + 9u > bytes.size()) { fail(error, "truncated GIF image descriptor"); return std::nullopt; }
        const int left = le16(bytes, pos + 0);
        const int top = le16(bytes, pos + 2);
        const int width = le16(bytes, pos + 4);
        const int height = le16(bytes, pos + 6);
        const auto image_packed = bytes[pos + 8];
        pos += 9;
        if (width <= 0 || height <= 0 || left < 0 || top < 0 ||
            left + width > screen_width || top + height > screen_height) {
            fail(error, "GIF image descriptor lies outside logical screen");
            return std::nullopt;
        }
        std::vector<std::uint16_t> local_palette;
        if ((image_packed & 0x80u) != 0) {
            const std::size_t table_entries = 1u << ((image_packed & 0x07u) + 1u);
            auto palette = read_palette555(bytes, pos, table_entries, error);
            if (!palette) return std::nullopt;
            local_palette = std::move(*palette);
        }
        const auto& active_palette = local_palette.empty() ? global_palette : local_palette;
        if (active_palette.empty()) {
            fail(error, "GIF image has no color table");
            return std::nullopt;
        }
        if (pos >= bytes.size()) { fail(error, "missing GIF LZW code size"); return std::nullopt; }
        const int minimum_code_size = bytes[pos++];
        auto compressed = read_sub_blocks(bytes, pos);
        if (!compressed) { fail(error, "truncated GIF image data blocks"); return std::nullopt; }
        auto indices = gif_lzw_decode(*compressed, minimum_code_size,
                                      static_cast<std::size_t>(width) * height, error);
        if (!indices) return std::nullopt;

        std::vector<int> row_order;
        row_order.reserve(height);
        if ((image_packed & 0x40u) == 0) {
            for (int y = 0; y < height; ++y) row_order.push_back(y);
        } else {
            for (int y = 0; y < height; y += 8) row_order.push_back(y);
            for (int y = 4; y < height; y += 8) row_order.push_back(y);
            for (int y = 2; y < height; y += 4) row_order.push_back(y);
            for (int y = 1; y < height; y += 2) row_order.push_back(y);
        }
        for (int source_row = 0; source_row < height; ++source_row) {
            const int dest_y = top + row_order[source_row];
            const auto source = indices->begin() + static_cast<std::ptrdiff_t>(source_row * width);
            auto dest = canvas.begin() + static_cast<std::ptrdiff_t>(dest_y * screen_width + left);
            auto rgb_dest = rgb555_canvas.begin() + static_cast<std::ptrdiff_t>(dest_y * screen_width + left);
            for (int x = 0; x < width; ++x) {
                const auto index = source[x];
                if (index >= active_palette.size()) {
                    fail(error, "GIF palette index is outside active color table");
                    return std::nullopt;
                }
                dest[x] = index;
                rgb_dest[x] = active_palette[index];
            }
        }
        decoded_image = true;
        // The original sprite plates are single-image GIFs. Decode the first
        // image deterministically rather than introducing animation semantics.
        break;
    }

    if (!decoded_image) {
        fail(error, "GIF contains no image");
        return std::nullopt;
    }
    return LegacyIndexedImage{
        screen_width, screen_height, screen_width, std::move(canvas), std::move(rgb555_canvas)};
}

std::optional<std::vector<LegacySpriteFrameMetadata>> extract_legacy_sprite_frames(
    const LegacyIndexedImage& image,
    std::string* error) {
    if (!valid_image(image) || image.width < 3 || image.height < 2) {
        fail(error, "sprite alpha plate has invalid dimensions/storage");
        return std::nullopt;
    }
    if (image.pixels[0] == image.pixels[1] || image.pixels[1] == image.pixels[2]) {
        fail(error, "sprite alpha plate marker header is invalid");
        return std::nullopt;
    }

    std::vector<LegacySpriteFrameMetadata> frames;
    int persistent_y = 0;
    int persistent_x = 0;

    for (;;) {
        int y = persistent_y;
        int x = persistent_x;
        if (y == 0) ++y;
        bool found = false;

        while (y < image.height) {
            auto band_height = marker_row_distance(image, y);
            if (!band_height) {
                ++y;
                continue;
            }

            while (x < image.width) {
                auto cell_width = marker_column_distance(image, y, *band_height, x);
                if (!cell_width) {
                    ++x;
                    continue;
                }

                auto trimmed = trim_marker_cell(image, x, y, *cell_width, *band_height);
                if (trimmed) {
                    persistent_y = y;
                    persistent_x = x + *cell_width + 1;

                    // 0x1F418..0x1F484 constructs the source Rect from the
                    // scan coordinates and the trimmed dimensions.
                    const int left = x + 1;
                    const int top = y + (*band_height - trimmed->height) - 1;
                    LegacySpriteRect rect{
                        left,
                        top,
                        left + trimmed->width,
                        top + trimmed->height,
                    };
                    LegacySpriteFrameMetadata frame;
                    frame.source_rect = rect;
                    frame.width = trimmed->width;
                    frame.height = trimmed->height;
                    frames.push_back(std::move(frame));
                    found = true;
                    break;
                }

                x += *cell_width + 1;
            }
            if (found) break;
            x = 0;
            y += *band_height + 1;
        }

        if (!found) break;
    }

    if (frames.empty()) {
        fail(error, "sprite alpha plate contains no frame rectangles");
        return std::nullopt;
    }
    if (frames.size() >= 0xffffu) {
        fail(error, "sprite alpha plate contains too many frames");
        return std::nullopt;
    }
    return frames;
}

std::optional<LegacySpriteGroupMetadata> build_legacy_sprite_group(
    FourCC id,
    const LegacyIndexedImage& alpha_plate,
    const LegacyIndexedImage& color_plate,
    std::string* error) {
    if (!valid_image(alpha_plate) || !valid_image(color_plate) ||
        alpha_plate.width != color_plate.width || alpha_plate.height != color_plate.height) {
        fail(error, "sprite alpha/color plates have incompatible dimensions/storage");
        return std::nullopt;
    }
    const auto alpha_storage = static_cast<std::size_t>(alpha_plate.row_bytes) * alpha_plate.height;
    const auto color_storage = static_cast<std::size_t>(color_plate.row_bytes) * color_plate.height;
    if (alpha_plate.rgb555_pixels.size() < alpha_storage ||
        color_plate.rgb555_pixels.size() < color_storage || color_plate.width < 3) {
        fail(error, "sprite plate is missing decoded xRGB1555 pixels");
        return std::nullopt;
    }

    auto scanned = extract_legacy_sprite_frames(alpha_plate, error);
    if (!scanned) return std::nullopt;

    // 0x18EB0..0x18EC0 reads the color plate's third 16-bit pixel and passes
    // it through to 0x1D780 as the frame's transparent color key.
    const auto transparent_key = color_plate.rgb555_pixels[2];
    LegacySpriteGroupMetadata group;
    group.id = id;
    group.frames.reserve(scanned->size());

    for (auto frame : *scanned) {
        const auto& r = frame.source_rect;
        if (r.left < 0 || r.top < 0 || r.right > alpha_plate.width || r.bottom > alpha_plate.height ||
            r.width() != frame.width || r.height() != frame.height || frame.width <= 0 || frame.height <= 0) {
            fail(error, "sprite frame rectangle lies outside matched plates");
            return std::nullopt;
        }

        const auto pixel_count = static_cast<std::size_t>(frame.width) * frame.height;
        frame.transparent_key = transparent_key;
        frame.color_pixels.reserve(pixel_count);
        frame.transparency.assign(pixel_count, 32u);
        bool any_nontransparent = false;

        for (int y = 0; y < frame.height; ++y) {
            bool row_has_nontransparent = false;
            for (int x = 0; x < frame.width; ++x) {
                const auto alpha_offset = static_cast<std::size_t>(r.top + y) * alpha_plate.row_bytes +
                                          static_cast<std::size_t>(r.left + x);
                const auto color_offset = static_cast<std::size_t>(r.top + y) * color_plate.row_bytes +
                                          static_cast<std::size_t>(r.left + x);
                const auto alpha_pixel = alpha_plate.rgb555_pixels[alpha_offset];
                frame.color_pixels.push_back(color_plate.rgb555_pixels[color_offset]);

                std::uint16_t transparency = 32u;
                if (alpha_pixel != transparent_key) {
                    const auto red5 = static_cast<std::uint16_t>((alpha_pixel >> 10u) & 0x1fu);
                    if (red5 < 31u) {
                        transparency = red5;
                        row_has_nontransparent = true;
                        any_nontransparent = true;
                    }
                }
                frame.transparency[static_cast<std::size_t>(y) * frame.width + x] = transparency;
            }
            // 0x1EFE4..0x1EFE8: a fully transparent row stores the 1000
            // sentinel in its first mask word so the blitters can skip it.
            if (!row_has_nontransparent) {
                frame.transparency[static_cast<std::size_t>(y) * frame.width] = 1000u;
            }
        }

        // 0x1EFFC..0x1F010 frees the whole plane when no pixel had a value
        // below 31. The frame then falls back to transparent-key blitting.
        if (!any_nontransparent) frame.transparency.clear();
        group.frames.push_back(std::move(frame));
    }
    return group;
}

std::pair<int, int> legacy_scaled_sprite_dimensions(
    const LegacySpriteFrameMetadata& frame,
    float scale) {
    if (scale == 1.0f) return {frame.width, frame.height};
    return {
        static_cast<int>(std::trunc(static_cast<float>(frame.width) * scale)),
        static_cast<int>(std::trunc(static_cast<float>(frame.height) * scale)),
    };
}

bool LegacySpriteCache::publish(LegacySpriteGroupMetadata group) {
    if (group.id == FourCC{} || group.id == fourcc('n', 'o', 'n', 'e') || group.frames.empty()) return false;
    const auto existing = std::find_if(groups_.begin(), groups_.end(), [&](const auto& candidate) {
        return candidate.id == group.id;
    });
    if (existing != groups_.end()) return true;
    groups_.push_back(std::move(group));
    return true;
}

const LegacySpriteFrameMetadata* LegacySpriteCache::find_loaded_frame(FourCC id, int frame) const {
    if (id == fourcc('n', 'o', 'n', 'e')) return nullptr;
    const auto it = std::find_if(groups_.begin(), groups_.end(), [&](const auto& candidate) {
        return candidate.id == id;
    });
    if (it == groups_.end() || it->frames.empty()) return nullptr;
    // The original compares signed frame >= count and normalizes that case to
    // zero. Negative indices would have indexed before the PPC pointer list;
    // the clean core rejects them rather than reproducing memory unsafety.
    if (frame < 0) return nullptr;
    if (static_cast<std::size_t>(frame) >= it->frames.size()) frame = 0;
    return &it->frames[static_cast<std::size_t>(frame)];
}

std::size_t LegacySpriteCache::frame_count(FourCC id) const {
    const auto it = std::find_if(groups_.begin(), groups_.end(), [&](const auto& candidate) {
        return candidate.id == id;
    });
    return it == groups_.end() ? 0u : it->frames.size();
}

std::pair<int, int> LegacySpriteCache::dimensions(
    FourCC id,
    int frame,
    float scale,
    const Loader& loader) {
    if (id == fourcc('n', 'o', 'n', 'e')) return {0, 0};
    if (const auto* loaded = find_loaded_frame(id, frame)) {
        return legacy_scaled_sprite_dimensions(*loaded, scale);
    }
    // 0x19CA0 first checks frame zero before requesting resource load. This
    // distinguishes an absent group from a high frame number normalized by
    // 0x19AD0.
    if (find_loaded_frame(id, 0)) return {0, 0};
    if (loader && loader(id, *this)) {
        if (const auto* loaded = find_loaded_frame(id, frame)) {
            return legacy_scaled_sprite_dimensions(*loaded, scale);
        }
    }
    return {0, 0};
}

} // namespace deimos
