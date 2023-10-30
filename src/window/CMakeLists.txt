add_library(starsight_window
        src/window.cpp
)
add_library(starsight::window ALIAS starsight_window)

target_include_directories(starsight_window
        INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include/window"
)

target_link_libraries(starsight_window PUBLIC starsight::core)
target_link_libraries(starsight_window PUBLIC glfw)