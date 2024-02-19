add_library(starsight_core
        src/math.cpp
        src/log.cpp
        src/assertion.cpp
        src/filesystem.cpp
        src/functions.asm
        src/time.cpp
        src/utility_functions.cpp
        src/resource.cpp
)

add_library(starsight::core ALIAS starsight_core)

target_include_directories(starsight_core
        INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include/core"
)

target_compile_definitions(starsight_core
        PRIVATE STARSIGHT_DIRECTORY="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(starsight_core
        PUBLIC pthread
        PUBLIC Taskflow
        PUBLIC fmt
        PUBLIC assert
        PUBLIC quill
        PUBLIC glm
        PUBLIC tbb
)