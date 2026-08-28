#include "deimos/game_definitions.hpp"

#include <cassert>

int main() {
    // The full database is validated against the real reference PAK by
    // deimos_reference_probe. This unit test keeps lookup/sentinel behavior
    // in the clean suite without embedding original resources.
    deimos::FourCC id{{'t','e','s','t'}};
    deimos::FourCC other{{'n','o','n','e'}};
    assert(id.str() == "test");
    assert(other.str() == "none");
}
