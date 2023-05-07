# lots of ugly hacks to make this work with cmake

cmake_minimum_required(VERSION 3.16)
project(libfsm)
set(CMAKE_C_FLAGS "-Wno-incompatible-function-pointer-types -Wno-int-conversion -Wno-incompatible-pointer-types")
set(CMAKE_C_STANDARD 11)
file(GLOB_RECURSE LIBFSM_SOURCES CONFIGURE_DEPENDS "src/libfsm/*.c" "src/fsm/*.c" "src/adt/*.c" "src/print/*.c")
file(GLOB_RECURSE LIBFSM_HEADERS CONFIGURE_DEPENDS "include/*.h")
add_custom_target(build_libfsm)
add_library(libfsm ${LIBFSM_SOURCES} ${LIBFSM_HEADERS})
add_library(libfsm::libfsm ALIAS libfsm)
add_dependencies(build_libfsm libfsm)
target_include_directories(libfsm PUBLIC src include)
target_compile_definitions(libfsm PRIVATE LX_HEADER="lexer.h")
