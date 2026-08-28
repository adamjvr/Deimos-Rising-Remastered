#include "deimos/film.hpp"

#include <cassert>
#include <cstdint>
#include <vector>

namespace {
void put_be32(std::vector<std::uint8_t>& b, std::size_t o, std::uint32_t v) {
    b[o] = static_cast<std::uint8_t>(v >> 24u);
    b[o + 1] = static_cast<std::uint8_t>(v >> 16u);
    b[o + 2] = static_cast<std::uint8_t>(v >> 8u);
    b[o + 3] = static_cast<std::uint8_t>(v);
}
void put_fourcc(std::vector<std::uint8_t>& b, std::size_t o, const char (&s)[5]) {
    for (std::size_t i = 0; i < 4; ++i) b[o + i] = static_cast<std::uint8_t>(s[i]);
}
}

int main() {
    using namespace deimos;
    std::vector<std::uint8_t> bytes(FilmV10005::file_size, 0);
    put_be32(bytes, 0x00, 10005);
    put_be32(bytes, 0x04, 0x11223344u);
    put_fourcc(bytes, 0x08, "le01");
    bytes[0x0c] = 1;
    put_be32(bytes, 0x10, 3);
    put_be32(bytes, 0x14, 0x55667788u);
    put_fourcc(bytes, 0x18, "le01");
    bytes[64] = input_bit_0 | input_bit_5;
    bytes[65] = input_bit_2;
    bytes[66] = input_bit_6;
    const auto tail = FilmV10005::header_size + FilmV10005::primary_input_capacity;
    bytes[tail + 100] = 'n'; bytes[tail + 101] = 'o'; bytes[tail + 102] = 'n'; bytes[tail + 103] = 'e';

    std::string error;
    const auto film = parse_film_v10005(bytes, &error);
    assert(film);
    assert(film->header.format_version == 10005);
    assert(film->header.level_id_a.str() == "le01");
    assert(film->header.recorded_ticks == 3);
    assert(film->primary_input.size() == 3);
    assert(film->primary_input[2] == input_bit_6);
    assert(film->primary_tail[100] == 'n');

    bytes[64] = 0x80;
    assert(!parse_film_v10005(bytes, &error));
    return 0;
}
