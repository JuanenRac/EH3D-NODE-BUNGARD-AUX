# EH3D-NODE-BUNGARD-AUX host tests: the settings file, the measure of the laser, the speed and the temperature of the spindle and the words of the network, built with any C++17
# compiler: cmake -S tests -B build/host && cmake --build build/host && ctest --test-dir build/host -C Debug
# Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
cmake_minimum_required(VERSION 3.16)
project(eh3d_node_bungard_aux_tests CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
  add_compile_options(/W4 /WX /utf-8)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

add_executable(test_aux test_aux.cpp)
add_executable(test_stream test_stream.cpp)

enable_testing()
add_test(NAME aux COMMAND test_aux)
add_test(NAME stream COMMAND test_stream)
