cmake_minimum_required(VERSION 3.16)
project(Vitality LANGUAGES CXX)

include(CTest)
enable_testing()

add_library(Vitality INTERFACE)
target_include_directories(Vitality INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_compile_features(Vitality INTERFACE cxx_std_20)

add_executable(vitality_basic_example examples/basic.cpp)
target_link_libraries(vitality_basic_example PRIVATE Vitality)

if(UNIX)
    add_executable(vitality_socket_example examples/socket.cpp)
    target_link_libraries(vitality_socket_example PRIVATE Vitality)

    add_executable(vitality_dispatch_example examples/dispatch.cpp)
    target_link_libraries(vitality_dispatch_example PRIVATE Vitality)

    add_executable(vitality_asio_multicast_dispatch_example EXCLUDE_FROM_ALL examples/asio_multicast_dispatch.cpp)
    target_link_libraries(vitality_asio_multicast_dispatch_example PRIVATE Vitality)
endif()

add_executable(vitality_tests tests/vitality_tests.cpp)
target_include_directories(vitality_tests PRIVATE tests)
target_link_libraries(vitality_tests PRIVATE Vitality)
add_test(NAME vitality_tests COMMAND vitality_tests)

add_executable(vitality_dropin_header_tests tests/dropin_header_tests.cpp)
target_include_directories(vitality_dropin_header_tests PRIVATE tests)
target_compile_features(vitality_dropin_header_tests PRIVATE cxx_std_20)
add_test(NAME vitality_dropin_header_tests COMMAND vitality_dropin_header_tests)
