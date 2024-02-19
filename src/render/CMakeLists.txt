find_package(Vulkan REQUIRED)

add_library(starsight_render
        src/vk_context.cpp
        src/vk_memory_allocator.cpp
        src/vk_model.cpp
        src/vk_pipeline.cpp
        src/vk_render_target.cpp
        src/vk_shader.cpp
        src/vk_descriptor.cpp
        src/spirv_reflect.cpp
        src/vk_utility.cpp
        src/image.cpp
        src/vk_buffer.cpp
)

add_library(starsight::render ALIAS starsight_render)

target_include_directories(starsight_render
        INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include/render"
        PRIVATE "${CMAKE_SOURCE_DIR}/third_party/stb"
)

target_compile_definitions(starsight_render
        PRIVATE VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
)

target_link_libraries(starsight_render
        PUBLIC starsight::core
        PUBLIC starsight::window
        PUBLIC concurrentqueue
        PUBLIC vulkan
        PUBLIC assimp
        PUBLIC shaderc_shared
        PUBLIC shaderc_util
        PUBLIC glslc
)