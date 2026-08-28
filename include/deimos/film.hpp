#pragma once

#include "deimos/resource_id.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace deimos {

struct FilmV10005Header {
    std::uint32_t format_version = 0;
    std::uint32_t field_04 = 0;       // Semantics not yet proven.
    FourCC level_id_a{};              // FourCC observed at file offset 0x08.
    std::uint8_t mode_byte = 0;       // Observed as 1 in all current one-player films.
    std::uint32_t recorded_ticks = 0; // Confirmed by zeroed input tail in the corpus.
    std::uint32_t field_14 = 0;       // Semantics not yet proven.
    FourCC level_id_b{};              // FourCC observed at file offset 0x18.
};

struct FilmV10005 {
    static constexpr std::size_t file_size = 40296;
    static constexpr std::size_t header_size = 64;
    static constexpr std::size_t primary_input_capacity = 20000;
    static constexpr std::size_t candidate_player_record_stride = 20116;

    FilmV10005Header header{};
    std::vector<std::uint8_t> primary_input;
    std::array<std::uint8_t, 116> primary_tail{};
};

// Low seven bits are the only bits observed in the supplied corpus. Exact
// direction/action names remain intentionally unassigned until PPC/2P evidence
// proves them. Corpus statistics strongly identify bits 0/1 and 2/3 as two
// opposing directional pairs; bit 6 is the rare action consistent with weapon
// switching.
enum FilmInputBit : std::uint8_t {
    input_bit_0 = 1u << 0u,
    input_bit_1 = 1u << 1u,
    input_bit_2 = 1u << 2u,
    input_bit_3 = 1u << 3u,
    input_bit_4 = 1u << 4u,
    input_bit_5 = 1u << 5u,
    input_bit_6 = 1u << 6u,
};

std::optional<FilmV10005> parse_film_v10005(
    std::span<const std::uint8_t> bytes,
    std::string* error = nullptr);

} // namespace deimos
