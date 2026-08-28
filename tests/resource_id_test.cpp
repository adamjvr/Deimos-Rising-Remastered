#include "deimos/resource_id.hpp"
#include <cassert>

int main() {
    using namespace deimos;

    const auto alpha = parse_resource_name("im08/Player 1 Blue IA[PL1B].gif");
    assert(alpha);
    assert(alpha->display_name == "Player 1 Blue");
    assert(alpha->tag.str() == "PL1B");
    assert(alpha->plate == PlateKind::alpha);
    assert(alpha->kind == ResourceKind::image8);

    const auto color = parse_resource_name("Player 1 Blue IC[pl1b].gif");
    assert(color && color->tag.str() == "pl1b");
    assert(color->plate == PlateKind::color);

    const auto level = parse_resource_name("leve/Level 01[le01].leve");
    assert(level && level->tag.str() == "le01");
    assert(level->kind == ResourceKind::level);

    const auto spaced = parse_resource_name("Bop[bop ].IMA");
    assert(spaced && spaced->tag.str() == "bop ");
    assert(spaced->kind == ResourceKind::audio);

    assert(!parse_resource_name("not-a-resource.gif"));
    return 0;
}
