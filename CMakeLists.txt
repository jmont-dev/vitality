cmake_minimum_required(VERSION 3.16)
project(Vitality LANGUAGES CXX)

include(CTest)

enable_testing()

add_library(Vitality INTERFACE)
target_include_directories(Vitality INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_compile_features(Vitality INTERFACE cxx_std_20)

add_executable(vitality_example examples/basic.cpp)
target_link_libraries(vitality_example PRIVATE Vitality)

add_executable(vitality_tests tests/vitality_tests.cpp)
target_include_directories(vitality_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
target_link_libraries(vitality_tests PRIVATE Vitality)
add_test(NAME vitality_tests COMMAND vitality_tests)
