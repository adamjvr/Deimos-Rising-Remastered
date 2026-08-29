#include "deimos/apple_metal_presentation_backend.hpp"

#include <type_traits>

int main() {
    static_assert(std::is_base_of_v<
        deimos::ModernPresentationBackend,
        deimos::AppleMetalPresentationBackend>);
    static_assert(!std::is_copy_constructible_v<deimos::AppleMetalPresentationBackend>);
    static_assert(!std::is_copy_assignable_v<deimos::AppleMetalPresentationBackend>);
    return 0;
}
