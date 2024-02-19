add_library(starsight_world
        src/world.cpp
        src/transform_component.cpp
        src/render_module.cpp
        src/audio_module.cpp
        src/input_module.cpp
        src/camera_component.cpp
        src/model_component.cpp
        src/mesh_component.cpp
        src/soundcue_component.cpp
)
add_library(starsight::world ALIAS starsight_world)

target_include_directories(starsight_world
        INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include/world"
)

target_link_libraries(starsight_world
        PUBLIC starsight::core
        PUBLIC starsight::render
        PUBLIC starsight::audio
        PUBLIC starsight::window
        PUBLIC flecs
        PUBLIC RapidJSON
)