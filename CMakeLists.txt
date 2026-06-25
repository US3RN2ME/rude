cmake_minimum_required(VERSION 3.28)

option(RUDE_BUILD_TESTS "Build tests" ON)
option(RUDE_BUILD_EXAMPLES "Build examples" ON)
option(RUDE_INSTALL "Generate install rules" ON)

if(RUDE_BUILD_TESTS)
   list(APPEND VCPKG_MANIFEST_FEATURES "tests")
endif()

project(
   rude
   VERSION 0.1.0
   DESCRIPTION "Reliable UDP Engine"
   LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

configure_file("${PROJECT_SOURCE_DIR}/cmake/version.hpp.in" "${PROJECT_BINARY_DIR}/include/rude/version.hpp" @ONLY)

add_subdirectory(src)

if(RUDE_BUILD_TESTS)
   enable_testing()
   add_subdirectory(tests)
endif()

if(RUDE_BUILD_EXAMPLES)
   add_subdirectory(examples)
endif()

if(RUDE_INSTALL)
   set(RUDE_INSTALL_CONFIGDIR "${CMAKE_INSTALL_LIBDIR}/cmake/rude")

   install(TARGETS rude EXPORT rudeTargets)

   install(
      DIRECTORY "${PROJECT_SOURCE_DIR}/src/rude"
      DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
      FILES_MATCHING
      PATTERN "*.hpp")

   install(FILES "${PROJECT_BINARY_DIR}/include/rude/version.hpp" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/rude")

   install(
      EXPORT rudeTargets
      NAMESPACE rude::
      DESTINATION "${RUDE_INSTALL_CONFIGDIR}")

   configure_package_config_file("${PROJECT_SOURCE_DIR}/cmake/rudeConfig.cmake.in" "${PROJECT_BINARY_DIR}/rudeConfig.cmake"
                                 INSTALL_DESTINATION "${RUDE_INSTALL_CONFIGDIR}")

   write_basic_package_version_file(
      "${PROJECT_BINARY_DIR}/rudeConfigVersion.cmake"
      VERSION "${PROJECT_VERSION}"
      COMPATIBILITY SameMajorVersion)

   install(FILES "${PROJECT_BINARY_DIR}/rudeConfig.cmake" "${PROJECT_BINARY_DIR}/rudeConfigVersion.cmake"
           DESTINATION "${RUDE_INSTALL_CONFIGDIR}")
endif()
