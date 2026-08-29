#include "deimos/apple_metal_host_view.hpp"

#include <type_traits>

int main() {
    static_assert(!std::is_copy_constructible_v<deimos::AppleMetalHostView>);
    static_assert(std::is_move_constructible_v<deimos::AppleMetalHostView>);
    static_assert(std::is_move_assignable_v<deimos::AppleMetalHostView>);
    static_assert(std::is_same_v<
        decltype(std::declval<const deimos::AppleMetalHostView&>().drawable_size()),
        deimos::ModernDrawableSize>);
    static_assert(std::is_same_v<
        decltype(std::declval<const deimos::AppleMetalHostView&>().native_view_handle()),
        void*>);
    static_assert(std::is_same_v<
        decltype(std::declval<const deimos::AppleMetalHostView&>().metal_layer_handle()),
        void*>);
    return 0;
}
