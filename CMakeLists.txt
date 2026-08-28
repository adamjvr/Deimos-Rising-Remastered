cmake_minimum_required(VERSION 3.20)
project(DeimosRisingRemastered LANGUAGES CXX)

add_library(deimos_core STATIC
    src/core/resource_id.cpp
)
target_include_directories(deimos_core PUBLIC include)
target_compile_features(deimos_core PUBLIC cxx_std_20)

include(CTest)
if(BUILD_TESTING)
    add_executable(resource_id_test tests/resource_id_test.cpp)
    target_link_libraries(resource_id_test PRIVATE deimos_core)
    add_test(NAME resource_id_test COMMAND resource_id_test)
endif()
