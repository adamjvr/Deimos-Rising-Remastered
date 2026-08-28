#include "deimos/definition_fields.hpp"

#include <cassert>
#include <cmath>

int main() {
    std::string error;
    deimos::TaggedRecord int_as_float{"defaultShieldPercentage_INT", "100.000000", "", 1, 0};
    auto f = deimos::parse_definition_field(int_as_float, &error);
    assert(f);
    assert(std::get<int>(f->value) == 100);

    deimos::DefinitionFieldSet fields;
    fields.add(std::move(*f));
    assert(fields.int_value("defaultShieldPercentage_INT") == 100);
    assert(fields.float_value("defaultShieldPercentage_INT") == 100.0f);

    deimos::TaggedRecord opaque_id{"Format_ID", "3", "", 2, 0};
    auto o = deimos::parse_definition_field(opaque_id, &error);
    assert(o);
    assert(std::get<std::string>(o->value) == "3");

    deimos::TaggedRecord fourcc{"drawLayer_ID", "air ", "", 3, 0};
    auto id = deimos::parse_definition_field(fourcc, &error);
    assert(id);
    assert(std::get<deimos::FourCC>(id->value).str() == "air ");
}
