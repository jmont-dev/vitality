cmake_minimum_required(VERSION 3.16)
project(Vitality LANGUAGES CXX)

include(CTest)
enable_testing()

add_library(Vitality INTERFACE)
target_include_directories(Vitality INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_compile_features(Vitality INTERFACE cxx_std_20)

add_executable(vitality_tests tests/vitality_tests.cpp)
target_include_directories(vitality_tests PRIVATE tests)
target_link_libraries(vitality_tests PRIVATE Vitality)
add_test(NAME vitality_tests COMMAND vitality_tests)

add_executable(vitality_basic_example examples/basic.cpp)
target_link_libraries(vitality_basic_example PRIVATE Vitality)

if(UNIX)
    add_executable(vitality_socket_example examples/socket.cpp)
    target_link_libraries(vitality_socket_example PRIVATE Vitality)
endif()
