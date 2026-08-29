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
    src/core/sprite_resource.cpp
    src/core/audio_resource.cpp
    src/core/game_definitions.cpp
)
target_include_directories(deimos_core PUBLIC include)
target_compile_features(deimos_core PUBLIC cxx_std_20)

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
        render_runtime_test
        render_backend_test
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
    endif()
endif()
