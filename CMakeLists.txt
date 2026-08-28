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
        data_tables_test)
        add_executable(${test_name} tests/${test_name}.cpp)
        target_link_libraries(${test_name} PRIVATE deimos_core)
        add_test(NAME ${test_name} COMMAND ${test_name})
    endforeach()
endif()
