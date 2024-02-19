add_library(starsight_audio
        src/audio_context.cpp
)

add_library(starsight::audio ALIAS starsight_audio)

target_include_directories(starsight_audio
        INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include/audio"
)

target_link_libraries(starsight_audio
        PUBLIC starsight::core
        PRIVATE vorbisfile
        PRIVATE steamaudio
)