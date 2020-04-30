cmake_minimum_required(VERSION 3.14)

project(ovk)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_definitions(_SILENCE_CLANG_CONCEPTS_MESSAGE)

set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory("ovk")
add_subdirectory("mightycity")
add_subdirectory("sandbox")
add_subdirectory("examples")
