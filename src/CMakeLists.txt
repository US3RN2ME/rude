file(GLOB_RECURSE RUDE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

find_package(boost_asio CONFIG REQUIRED)

add_library(rude)

add_library(rude::rude ALIAS rude)

target_sources(rude PRIVATE ${RUDE_SOURCES})

target_compile_features(rude PUBLIC cxx_std_23)

target_link_libraries(rude PUBLIC Boost::asio)

target_include_directories(
   rude PUBLIC $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include> $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
               $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
