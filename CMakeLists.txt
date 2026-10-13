cmake_minimum_required(VERSION 3.12)
project(xROUTE VERSION 2.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(ENABLE_ASAN "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
if(ENABLE_ASAN)
    if(MSVC)
        add_compile_options(/fsanitize=address)
    else()
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address,undefined")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address,undefined")
    endif()
endif()

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
