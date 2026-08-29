#pragma once

#include "deimos/render_backend.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace deimos {

// Decode the uncompressed 16-bit TGA family used by Deimos Rising interface
// surfaces (notably im16/Scorebar[scor].TGA). The returned surface is normalized
// to top-left origin and stores the low 15 xRGB1555 color bits consumed by the
// recovered software compositor.
[[nodiscard]] std::optional<LegacyRasterSurface> decode_legacy_tga16(
    std::span<const std::uint8_t> bytes,
    std::string* error = nullptr);

} // namespace deimos
