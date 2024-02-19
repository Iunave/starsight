#ifndef STARSIGHT_INPUT_MODULE_HPP
#define STARSIGHT_INPUT_MODULE_HPP

#include "flecs.h"
#include "camera_component.hpp"

class InputModule
{
private:
    static void PollWindowEvents(flecs::iter& it);
    static void UpdateCameras(flecs::iter& it, size_t, CameraComponent& Camera);

public:
    static inline constinit InputModule* Self = nullptr;

    glm::ivec2 FramebufferSize{};
    glm::dvec2 CursorPos{};
    glm::dvec2 PrevCursorPos{};

public:
    explicit InputModule(flecs::world& world);
    ~InputModule();
};

#endif //STARSIGHT_INPUT_MODULE_HPP
