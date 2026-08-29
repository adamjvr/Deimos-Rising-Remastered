cmake_minimum_required(VERSION 3.20)
project(DeimosRisingRemastered LANGUAGES CXX)

add_library(deimos_core STATIC
    src/core/resource_id.cpp
    src/core/legacy_text.cpp
    src/core/level.cpp
    src/core/film.cpp
    src/core/pak_archive.cpp
    src/core/resource_store.cpp
    src/core/data_tables.cpp
    src/core/definition_fields.cpp
    src/core/unit_definition.cpp
    src/core/weapon_definition.cpp
    src/core/player_definition.cpp
    src/core/player_runtime.cpp
    src/core/unit_behavior.cpp
    src/core/state_runtime.cpp
    src/core/legacy_math.cpp
    src/core/spawn_runtime.cpp
    src/core/entity_runtime.cpp
    src/core/entity_world.cpp
    src/core/collision_runtime.cpp
    src/core/destruction_runtime.cpp
    src/core/terrain_runtime.cpp
    src/core/render_runtime.cpp
    src/core/render_backend.cpp
    src/core/render_orchestration.cpp
    src/core/particle_runtime.cpp
    src/core/world_render_runtime.cpp
    src/core/presentation_runtime.cpp
    src/core/score_bar_runtime.cpp
    src/core/image16_resource.cpp
    src/core/gameplay_frame_runtime.cpp
    src/core/level_select_effect_runtime.cpp
    src/core/modern_presentation_runtime.cpp
    src/core/sprite_resource.cpp
    src/core/audio_resource.cpp
    src/core/game_definitions.cpp
)
target_include_directories(deimos_core PUBLIC include)
target_compile_features(deimos_core PUBLIC cxx_std_20)

# Native Apple presenter is intentionally a separate target so deimos_core
# stays platform-independent and deterministic. The adapter is available for
# both macOS and iPadOS toolchains when building on Apple platforms.
option(DEIMOS_BUILD_APPLE_METAL "Build the macOS/iPadOS Metal presentation backend" ON)
if(APPLE AND DEIMOS_BUILD_APPLE_METAL)
    enable_language(OBJCXX)

    add_library(deimos_metal_backend STATIC
        src/platform/apple/apple_metal_presentation_backend.mm)
    target_include_directories(deimos_metal_backend PUBLIC include)
    target_compile_features(deimos_metal_backend PUBLIC cxx_std_20)
    target_compile_options(deimos_metal_backend PRIVATE -fobjc-arc)
    target_compile_definitions(deimos_metal_backend PUBLIC DEIMOS_HAS_APPLE_METAL_BACKEND=1)

    find_library(DEIMOS_METAL_FRAMEWORK Metal REQUIRED)
    find_library(DEIMOS_QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
    find_library(DEIMOS_FOUNDATION_FRAMEWORK Foundation REQUIRED)
    target_link_libraries(deimos_metal_backend
        PUBLIC deimos_core
        PRIVATE
            ${DEIMOS_METAL_FRAMEWORK}
            ${DEIMOS_QUARTZCORE_FRAMEWORK}
            ${DEIMOS_FOUNDATION_FRAMEWORK})

    option(DEIMOS_BUILD_APPLE_HOST "Build the reusable macOS/iPadOS native Metal host view" ON)
    if(DEIMOS_BUILD_APPLE_HOST)
        # Reusable AppKit/UIKit host view. This is intentionally separate from the
    # Metal renderer so applications can own lifecycle/navigation while this
    # adapter owns only the native view + CAMetalLayer presentation surface.
        add_library(deimos_apple_host STATIC
            src/platform/apple/apple_metal_host_view.mm)
        target_include_directories(deimos_apple_host PUBLIC include)
        target_compile_features(deimos_apple_host PUBLIC cxx_std_20)
        target_compile_options(deimos_apple_host PRIVATE -fobjc-arc)
        target_compile_definitions(deimos_apple_host PUBLIC DEIMOS_HAS_APPLE_HOST_VIEW=1)
        target_link_libraries(deimos_apple_host
            PUBLIC deimos_metal_backend
            PRIVATE ${DEIMOS_QUARTZCORE_FRAMEWORK} ${DEIMOS_FOUNDATION_FRAMEWORK})

        if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
            find_library(DEIMOS_UIKIT_FRAMEWORK UIKit REQUIRED)
            target_link_libraries(deimos_apple_host PRIVATE ${DEIMOS_UIKIT_FRAMEWORK})
        else()
            find_library(DEIMOS_APPKIT_FRAMEWORK AppKit REQUIRED)
            target_link_libraries(deimos_apple_host PRIVATE ${DEIMOS_APPKIT_FRAMEWORK})
        endif()

        option(DEIMOS_BUILD_APPLE_SMOKE_APP "Build the native Apple Metal host smoke application" OFF)
        if(DEIMOS_BUILD_APPLE_SMOKE_APP)
            add_executable(deimos_apple_host_smoke MACOSX_BUNDLE
                src/platform/apple/apple_host_smoke_app.mm)
            target_compile_features(deimos_apple_host_smoke PRIVATE cxx_std_20)
            target_compile_options(deimos_apple_host_smoke PRIVATE -fobjc-arc)
            target_link_libraries(deimos_apple_host_smoke PRIVATE deimos_apple_host)
            set_target_properties(deimos_apple_host_smoke PROPERTIES
                MACOSX_BUNDLE_BUNDLE_NAME "Deimos Rising Metal Host Smoke"
                MACOSX_BUNDLE_GUI_IDENTIFIER "com.deimosrising.remastered.hostsmoke"
                XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.deimosrising.remastered.hostsmoke")
        endif()
    endif()
endif()

option(DEIMOS_BUILD_TOOLS "Build reverse-engineering/reference validation tools" ON)
if(DEIMOS_BUILD_TOOLS)
    add_executable(deimos_reference_probe tools/reference_probe.cpp)
    target_link_libraries(deimos_reference_probe PRIVATE deimos_core)
endif()

if(MSVC)
    target_compile_options(deimos_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(deimos_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    foreach(test_name IN ITEMS
        resource_id_test
        legacy_text_test
        level_test
        film_test
        pak_archive_test
        resource_store_test
        data_tables_test
        definition_fields_test
        unit_definition_test
        weapon_definition_test
        player_definition_test
        player_runtime_test
        player_lifecycle_test
        unit_behavior_test
        state_runtime_test
        legacy_math_test
        spawn_runtime_test
        entity_runtime_test
        entity_world_test
        collision_runtime_test
        core_edge_runtime_test
        destruction_runtime_test
        terrain_runtime_test
        terrain_surface_runtime_test
        render_runtime_test
        render_backend_test
        render_orchestration_test
        particle_runtime_test
        particle_lifecycle_test
        world_render_runtime_test
        presentation_runtime_test
        score_bar_runtime_test
        score_bar_pixel_runtime_test
        gameplay_frame_runtime_test
        level_select_effect_runtime_test
        modern_presentation_runtime_test
        apple_metal_public_header_test
        apple_metal_host_view_public_header_test
        shadow_runtime_test
        sprite_resource_test
        sprite_frame_bitmap_test
        audio_resource_test
        player_motion_test
        game_definitions_test)
        add_executable(${test_name} tests/${test_name}.cpp)
        target_link_libraries(${test_name} PRIVATE deimos_core)
        add_test(NAME ${test_name} COMMAND ${test_name})
    endforeach()

    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(
            NAME pef_probe_test
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/pef_probe_test.py)
        add_test(
            NAME apple_objcxx_layout_test
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/apple_objcxx_layout_test.py)
    endif()
endif()
