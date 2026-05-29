cmake_minimum_required(VERSION 3.25)

project(itercpp VERSION 0.1.0 LANGUAGES CXX)

option(ITERCPP_BUILD_EXAMPLES "Build example programs" ON)

add_library(itercpp INTERFACE)
target_include_directories(itercpp INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_compile_features(itercpp INTERFACE cxx_std_23)

include(CTest)

if(BUILD_TESTING)
  find_package(GTest CONFIG REQUIRED)

  add_executable(itercpp_tests
    tests/test_itercpp.cpp
  )
  target_link_libraries(itercpp_tests PRIVATE itercpp GTest::gtest_main)

  include(GoogleTest)
  gtest_discover_tests(itercpp_tests)
endif()

if(ITERCPP_BUILD_EXAMPLES)
  add_executable(itercpp_demo examples/demo.cpp)
  target_link_libraries(itercpp_demo PRIVATE itercpp)
endif()

install(DIRECTORY include/ DESTINATION include)
install(TARGETS itercpp EXPORT itercppTargets)
install(
  EXPORT itercppTargets
  FILE itercppTargets.cmake
  NAMESPACE itercpp::
  DESTINATION lib/cmake/itercpp
)

if(CMAKE_EXPORT_COMPILE_COMMANDS)
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E rm -f
            ${PROJECT_SOURCE_DIR}/compile_commands.json
    )

    execute_process(
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            ${CMAKE_BINARY_DIR}/compile_commands.json
            ${PROJECT_SOURCE_DIR}/compile_commands.json
    )
endif()