#include <iostream>
#include <pthread.h>
#include "core/log.hpp"
#include "core/time.hpp"
#include "core/filesystem.hpp"
#include "audio/audio_context.hpp"
#include "core/utility_functions.hpp"
#include "window/window.hpp"
#include "render/vk_context.hpp"
#include "render/vk_model.hpp"
#include "render/vk_render_target.hpp"
#include "world/world.hpp"
#include "world/transform_component.hpp"
#include "world/render_module.hpp"
#include "world/audio_module.hpp"
#include "world/input_module.hpp"

int main()
{
    global::MainThreadID = pthread_self();
    pthread_setname_np(pthread_self(), "main");

    GlfwInitialize();
    global::Window = GlfwCreateWindow("starsight");
    glfwPollEvents();

    vkContext = new VStarSightRenderer{global::Window};
    alContext = new AContext{};
    std::optional<flecs::world> World = CreateWorld();

    CameraComponent Camera{};
    Camera.Mode = CameraComponent::fly;
    Camera.Location = {0, 0, 0};

    auto CameraEntity = World->entity()
    .set<CameraComponent>(Camera);

    vkContext->Camera = CameraEntity.get<CameraComponent>();

    World->entity()
            .set<ModelComponent>({
                .Asset = ProjectAbsolutePath("assets/models/space_rock.gltf"),
                .isBuilt = false})
            .set<TransformComponent>(TransformComponent{
                    .location = {0,0,0},
                    .rotation = glm::angleAxis(M_PI / 2.0, axis::right),
                    .scale = {1,1,1}
            });

    for(uint64_t i = 0; i < 100; i++)
    {
        World->entity()
        .set<ModelComponent>({
            .Asset = ProjectAbsolutePath("assets/models/cube.gltf"),
            .isBuilt = false
        })
        .set<TransformComponent>({
            .location = math::RandVector(glm::dvec3{-1000.0}, glm::dvec3{1000.0}),
            .rotation = glm::identity<glm::dquat>(),
            .scale = math::RandVector(glm::dvec3{0.001}, glm::dvec3{1.0})
        });
    }

    while(!glfwWindowShouldClose(global::Window))
    {
        global::ProgramTime.StartFrame();
        World->progress(global::ProgramTime.FloatDelta);
        global::ProgramTime.EndFrame();
    }

    World.reset();
    SafeDelete(alContext)
    SafeDelete(vkContext)

    GlfwCloseWindow(global::Window);
    GlfwTerminate();

    return 0;
}