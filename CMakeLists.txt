cmake_minimum_required(VERSION 3.22)

set(CMAKE_C_COMPILER clang-18)
set(CMAKE_CXX_COMPILER clang++-18)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_FLAGS "-O3 -flto=full -march=native")

project(starsight)

add_executable(starsight src/main.cpp)
