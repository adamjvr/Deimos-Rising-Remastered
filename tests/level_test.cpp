#include "deimos/level.hpp"

#include <cassert>
#include <string>

int main() {
    using namespace deimos;
    const std::string plain =
        "#name_STR <Synthetic Valley>\r"
        "#indentifier_STR <Fixture>\r"
        "#description_STR <Synthetic parser regression data.>\r"
        "#copyright_STR <Test fixture.>\r"
        "#background_RECT <0, 0, 480, 3600>\r"
        "#backgroundImage_ID <sybg>\r"
        "#previewImage_ID <sypv>\r"
        "#music_ID <mu03>\r"
        "#mediaMask_ID <symd>\r"
        "#briefing_ID <none>\r"
        "#numObjects_INT <2>\r"
        "#unit_ID <un01>\r"
        "#layer_ID <air >\r"
        "#xLoc_INT <208>\r"
        "#yLoc_INT <330>\r"
        "#headingDegrees_INT <90>\r"
        "#isStationary_BOOL <FALSE>\r"
        "#enableTerrainEffects_BOOL <FALSE>\r"
        "#unit_ID <un02>\r"
        "#layer_ID <grnd>\r"
        "#xLoc_INT <100>\r"
        "#yLoc_INT <200>\r"
        "#headingDegrees_INT <180>\r"
        "#isStationary_BOOL <TRUE>\r"
        "#enableTerrainEffects_BOOL <TRUE>\r";

    const auto bytes = encode_legacy_text_canonical(plain);
    std::string error;
    const auto level = decode_and_parse_level(bytes, &error);
    assert(level);
    assert(level->name == "Synthetic Valley");
    assert(level->identifier == "Fixture");
    assert((level->background == RectI{0, 0, 480, 3600}));
    assert(level->objects.size() == 2);
    assert(level->objects[0].layer_id.str() == "air ");
    assert(level->objects[0].heading_degrees == 90);
    assert(!level->objects[0].stationary);
    assert(level->objects[1].unit_id.str() == "un02");
    assert(level->objects[1].terrain_effects);
    return 0;
}
