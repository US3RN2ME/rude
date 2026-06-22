option(BOOST_UT_ENABLE_RUN_AFTER_BUILD
       "Automatically run built artifacts. If disabled, the tests can be run with ctest instead" ON)

function(ut_add_custom_command_or_test)
   set(prefix "PARSE")
   set(noValues "")
   set(singleValues TARGET)
   set(multiValues COMMAND)

   cmake_parse_arguments("${prefix}" "${noValues}" "${singleValues}" "${multiValues}" ${ARGN})
   target_link_libraries(${PARSE_TARGET} PRIVATE Boost::ut)

   if(BOOST_UT_ENABLE_RUN_AFTER_BUILD)
      add_custom_command(
         TARGET ${PARSE_TARGET}
         COMMAND ${PARSE_COMMAND}
         POST_BUILD)
   else()
      add_test(NAME ${PARSE_TARGET} COMMAND ${PARSE_COMMAND})
   endif()
endfunction()

function(ut_add_run_all_target)
   set(prefix "PARSE")
   set(noValues "")
   set(singleValues TARGET_NAME)
   set(multiValues TARGETS)

   cmake_parse_arguments("${prefix}" "${noValues}" "${singleValues}" "${multiValues}" ${ARGN})

   if(BOOST_UT_ENABLE_RUN_AFTER_BUILD)
      add_custom_target(${PARSE_TARGET_NAME} DEPENDS ${PARSE_TARGETS})
   else()
      add_custom_target(
         ${PARSE_TARGET_NAME}
         COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
         DEPENDS ${PARSE_TARGETS})
   endif()
endfunction()
