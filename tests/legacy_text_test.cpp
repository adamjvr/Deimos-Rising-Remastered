#include "deimos/legacy_text.hpp"

#include <cassert>
#include <cmath>
#include <string>

int main() {
    using namespace deimos;

    for (unsigned value = 0; value < 128; ++value) {
        const auto encoded = encode_legacy_byte_canonical(static_cast<std::uint8_t>(value));
        assert(decode_legacy_byte(encoded) == value);
        assert(decode_legacy_byte(static_cast<std::uint8_t>(encoded ^ 0xffu)) == value);
    }

    const std::string plain =
        "// fixture\r"
        "#name_STR <Synthetic>\r"
        "#layer_ID <air >\r"
        "#enabled_BOOL <TRUE> // proven typed helper\r"
        "#count_INT <-42>\r"
        "#scale_FLOAT <0.96>\r"
        "#rect_RECT <0, 1, 480, 3600>\r"
        "#color_COLOR <52c594>\r";

    const auto encoded = encode_legacy_text_canonical(plain);
    assert(decode_legacy_text(encoded) == plain);

    std::string error;
    const auto doc = parse_tagged_text(plain, &error);
    assert(doc);
    assert(doc->records.size() == 7);
    assert(doc->records[2].inline_comment == "proven typed helper");
    assert(parse_id_value(doc->records[1].value)->str() == "air ");
    assert(*parse_bool_value(doc->records[2].value));
    assert(*parse_int_value(doc->records[3].value) == -42);
    assert(std::fabs(*parse_float_value(doc->records[4].value) - 0.96f) < 0.0001f);
    assert((*parse_rect_value(doc->records[5].value) == RectI{0, 1, 480, 3600}));
    assert((*parse_rgb24_value(doc->records[6].value) == Rgb24{0x52, 0xc5, 0x94}));

    const auto strings = parse_tagged_text("First\rSecond\rThird\r", &error);
    assert(strings && strings->records.empty() && strings->bare_lines.size() == 3);
    return 0;
}
