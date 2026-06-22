file(GLOB EXAMPLE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

foreach(EXAMPLE_SOURCE IN LISTS EXAMPLE_SOURCES)
   get_filename_component(EXAMPLE_NAME "${EXAMPLE_SOURCE}" NAME_WE)
   set(TARGET "rude_example_${EXAMPLE_NAME}")

   add_executable(${TARGET} ${EXAMPLE_SOURCE})

   target_link_libraries(${TARGET} PRIVATE rude::rude)

   target_compile_features(${TARGET} PRIVATE cxx_std_23)
endforeach()
