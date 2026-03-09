cmake_minimum_required(VERSION 3.16)
project(Vitality LANGUAGES CXX)

add_library(Vitality INTERFACE)
target_include_directories(Vitality INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_compile_features(Vitality INTERFACE cxx_std_20)

add_executable(vitality_example examples/basic.cpp)
target_link_libraries(vitality_example PRIVATE Vitality)
