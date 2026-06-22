cmake_minimum_required(VERSION 3.28)

option(RUDE_BUILD_TESTS "Build tests" ON)
option(RUDE_BUILD_EXAMPLES "Build examples" ON)

if(RUDE_BUILD_TESTS)
   list(APPEND VCPKG_MANIFEST_FEATURES "tests")
endif()

project(
   rude
   VERSION 0.1.0
   DESCRIPTION "Reliable UDP Engine"
   LANGUAGES CXX)

configure_file("${CMAKE_SOURCE_DIR}/cmake/version.hpp.in" "${PROJECT_BINARY_DIR}/include/rude/version.hpp" @ONLY)

add_subdirectory(src)

if(RUDE_BUILD_TESTS)
   enable_testing()
   add_subdirectory(tests)
endif()

if(RUDE_BUILD_EXAMPLES)
   add_subdirectory(examples)
endif()
