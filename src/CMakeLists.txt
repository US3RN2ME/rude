file(GLOB_RECURSE RUDE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

find_package(boost_asio CONFIG REQUIRED)

add_library(rude INTERFACE)

add_library(rude::rude ALIAS rude)

target_sources(rude INTERFACE ${RUDE_SOURCES})

target_compile_features(rude INTERFACE cxx_std_23)

target_link_libraries(rude INTERFACE Boost::asio)

target_include_directories(
   rude INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src> $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
                  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
