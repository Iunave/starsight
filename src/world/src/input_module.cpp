#include "input_module.hpp"
#include "window/window.hpp"

InputModule::InputModule(flecs::world& world)
{
    Self = this;

    world.module<InputModule>("Input");

    world.component<CameraComponent>("Camera");

    world.system("Poll Window Events")
            .kind(flecs::OnLoad)
            .iter(PollWindowEvents);
    
    world.system<CameraComponent>("Update Cameras")
            .kind(flecs::PostLoad)
            .each(UpdateCameras);
}

InputModule::~InputModule()
{
    Self = nullptr;
}

void InputModule::PollWindowEvents(flecs::iter& it)
{
    glfwPollEvents();

    Self->PrevCursorPos = Self->CursorPos;
    glfwGetCursorPos(global::Window, &Self->CursorPos.x, &Self->CursorPos.y);

    glfwGetFramebufferSize(global::Window, &Self->FramebufferSize.x, &Self->FramebufferSize.y);
}

void InputModule::UpdateCameras(flecs::iter& it, size_t, CameraComponent& Camera)
{
    if(glfwGetInputMode(global::Window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED)
    {
        if(Camera.Mode == CameraComponent::fly)
        {
            glm::dvec2 DeltaCursor = Self->PrevCursorPos - Self->CursorPos;
            glm::dvec2 RotationAngles = DeltaCursor * Camera.LookSpeed;

            Camera.Rotate(RotationAngles.x, axis::up);
            Camera.Rotate(RotationAngles.y, Camera.Rotation * axis::right);

            if(!Camera.LockRoll && glfwGetKey(global::Window, GLFW_KEY_E) == GLFW_PRESS)
            {
                Camera.Rotate(-Camera.RollSpeed * it.delta_time(), Camera.Rotation * axis::forward);
            }
            if(!Camera.LockRoll && glfwGetKey(global::Window, GLFW_KEY_Q) == GLFW_PRESS)
            {
                Camera.Rotate(Camera.RollSpeed * it.delta_time(), Camera.Rotation * axis::forward);
            }
            if(glfwGetKey(global::Window, GLFW_KEY_W) == GLFW_PRESS)
            {
                Camera.Location += (Camera.Rotation * axis::forward) * Camera.MoveSpeed * it.delta_time();
            }
            if(glfwGetKey(global::Window, GLFW_KEY_S) == GLFW_PRESS)
            {
                Camera.Location += (Camera.Rotation * axis::backward) * Camera.MoveSpeed * it.delta_time();
            }
            if(glfwGetKey(global::Window, GLFW_KEY_D) == GLFW_PRESS)
            {
                Camera.Location += (Camera.Rotation * axis::right) * Camera.MoveSpeed * it.delta_time();
            }
            if(glfwGetKey(global::Window, GLFW_KEY_A) == GLFW_PRESS)
            {
                Camera.Location += (Camera.Rotation * axis::left) * Camera.MoveSpeed * it.delta_time();
            }
            if(glfwGetKey(global::Window, GLFW_KEY_SPACE) == GLFW_PRESS)
            {
                Camera.Location += axis::up * Camera.MoveSpeed * it.delta_time();
            }
            if(glfwGetKey(global::Window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
            {
                Camera.Location += axis::down * Camera.MoveSpeed * it.delta_time();
            }
        }
        else if(Camera.Mode == CameraComponent::orbit)
        {
            if(glfwGetMouseButton(global::Window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
            {
                glm::dvec2 DeltaCursor = Self->PrevCursorPos - Self->CursorPos;
                DeltaCursor *= 0.001;

                glm::dquat HorizontalRotation = glm::angleAxis(DeltaCursor.x, axis::up);
                //glm::dquat VerticalRotation = glm::angleAxis(DeltaCursor.y, axis::right);
                glm::dquat Rotation = HorizontalRotation;

                Camera.Location = Rotation * Camera.Location;
                Camera.Rotation = glm::quatLookAt(-glm::normalize(Camera.Location), axis::up);
            }
        }
    }
}

