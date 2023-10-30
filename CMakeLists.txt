cmake_minimum_required(VERSION 3.22)

set(CMAKE_C_COMPILER clang-18)
set(CMAKE_CXX_COMPILER clang++-18)
set(CMAKE_CXX_STANDARD 23)

set(CMAKE_ASM_COMPILER nasm)
set(CMAKE_ASM_FLAGS "-F dwarf -f elf64")

project(starsight VERSION 1.0.0 LANGUAGES CXX ASM_NASM)

cmake_policy(SET CMP0077 NEW)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif ()

string(TOLOWER "${CMAKE_BUILD_TYPE}" BUILD_TYPE)

add_executable(starsight src/main.cpp)

if ("${BUILD_TYPE}" STREQUAL "debug")
    message(STATUS "compiling with debug info")
    set(CMAKE_CXX_FLAGS "-g -std=c++23 -O3 -flto=full -march=native")
elseif ("${BUILD_TYPE}" STREQUAL "release")
    message(STATUS "compiling in release mode")
    set(CMAKE_CXX_FLAGS "-DNDEBUG=1 -std=c++23 -O3 -flto=full -march=native")
else ()
    message(FATAL_ERROR "unknown build type")
endif ()

set(SOURCE_DIRECTORY "src")
set(THIRD_PARTY_DIRECTORY "${CMAKE_SOURCE_DIR}/third_party")

option(BUILD_SHARED_LIBS ON)

#third party libraries
add_subdirectory("${THIRD_PARTY_DIRECTORY}/fmt")
add_subdirectory("${THIRD_PARTY_DIRECTORY}/libassert")
option(QUILL_FMT_EXTERNAL ON)
add_subdirectory("${THIRD_PARTY_DIRECTORY}/quill")
add_subdirectory("${THIRD_PARTY_DIRECTORY}/glm")
add_subdirectory("${THIRD_PARTY_DIRECTORY}/glfw")

include_directories("${THIRD_PARTY_DIRECTORY}")
#subprojects
add_subdirectory("${SOURCE_DIRECTORY}/core")
add_subdirectory("${SOURCE_DIRECTORY}/window")
add_subdirectory("${SOURCE_DIRECTORY}/render")

#link
target_link_libraries(starsight
        PRIVATE starsight::core
        PRIVATE starsight::window
        PRIVATE starsight::render
)

target_include_directories(starsight
        PRIVATE "${PROJECT_BINARY_DIR}"
)
