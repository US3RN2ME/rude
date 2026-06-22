find_package(ut CONFIG REQUIRED)

find_program(BOOST_UT_MEMORYCHECK_COMMAND valgrind)

include(${CMAKE_SOURCE_DIR}/cmake/AddCustomCommandOrTest.cmake)

file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

set(ALL_TEST_TARGETS "")
foreach(TEST_SOURCE IN LISTS TEST_SOURCES)
   get_filename_component(TEST_NAME "${TEST_SOURCE}" NAME_WE)
   set(TARGET "rude_${TEST_NAME}")

   add_executable(${TARGET} ${TEST_SOURCE})
   target_link_libraries(${TARGET} PRIVATE rude::rude)

   ut_add_custom_command_or_test(TARGET ${TARGET} COMMAND ${TARGET})
   list(APPEND ALL_TEST_TARGETS ${TARGET})
endforeach()

ut_add_run_all_target(TARGET_NAME vigil_test_all TARGETS ${ALL_TEST_TARGETS})
