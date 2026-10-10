cmake_minimum_required(VERSION 3.12)
project(xROUTE VERSION 2.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

include_directories(include)

set(XROUTE_SOURCES
    src/map.c
    src/router.c
    src/parsers.c
    src/visualizer.c
)

add_library(xroute_static STATIC ${XROUTE_SOURCES})
set_target_properties(xroute_static PROPERTIES OUTPUT_NAME xroute)

add_library(xroute_shared SHARED ${XROUTE_SOURCES})
set_target_properties(xroute_shared PROPERTIES OUTPUT_NAME xroute)

add_executable(xroute_cli cli/main.c)
target_link_libraries(xroute_cli PRIVATE xroute_static m)

enable_testing()
add_executable(xroute_tests tests/test_main.c)
target_link_libraries(xroute_tests PRIVATE xroute_static m)
add_test(NAME xrouteUnitTests COMMAND xroute_tests)
