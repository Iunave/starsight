cmake_minimum_required(VERSION 3.22)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_ASM_COMPILER nasm)

set(CMAKE_EXPORT_COMPILE_COMMANDS OFF)
set(CMAKE_VERBOSE_MAKEFILE OFF)

cmake_policy(SET CMP0077 NEW)
cmake_policy(SET CMP0048 NEW)

project(starsight VERSION 1.0.0 LANGUAGES CXX ASM_NASM)

set(CMAKE_ASM_FLAGS "-F dwarf -f elf64")
set(COMMON_FLAGS "-std=c++23 -O3 -fPIC -march=native")
set(CMAKE_CXX_FLAGS_DEBUG "${COMMON_FLAGS} -g")
set(CMAKE_CXX_FLAGS_RELEASE "${COMMON_FLAGS} -DNDEBUG=1 -flto=full")

option(INSTALL_DOCS "Install documentation" ON)
option(INSTALL_PKG_CONFIG_MODULE "Install pkg config files" OFF)
option(INSTALL_CMAKE_PACKAGE_MODULE "Install CMake package configuration module" OFF)
option(BUILD_SHARED_LIBS "Build shared libraries" ON)

#third party submodules
add_subdirectory(third_party)

#starsight libraries
include_directories(third_party)

add_subdirectory(src/core)
add_subdirectory(src/window)
add_subdirectory(src/render)
add_subdirectory(src/audio)
add_subdirectory(src/world)

add_executable(starsight src/main.cpp)

#link
target_link_libraries(starsight
        PRIVATE starsight::core
        PRIVATE starsight::window
        PRIVATE starsight::render
        PRIVATE starsight::audio
        PRIVATE starsight::world
)

target_include_directories(starsight
        PRIVATE "${PROJECT_BINARY_DIR}"
)
