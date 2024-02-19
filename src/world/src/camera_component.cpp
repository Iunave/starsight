#include "camera_component.hpp"
#include <numbers>

CameraComponent::CameraComponent()
    : Location(0,0,0)
    , Rotation(glm::identity<glm::dquat>())
    , FieldOfView(std::numbers::pi / 2.0)
    , Near(0.1)
    , Far(10000.0)
    , MoveSpeed(10.0)
    , LookSpeed(0.001)
    , RollSpeed(1.0)
    , LockRoll(true)
{

}

void CameraComponent::Rotate(double Angle, glm::dvec3 Axis)
{
    Rotation = glm::normalize(glm::angleAxis(Angle, Axis) * Rotation);
}

glm::dmat4x4 CameraComponent::MakeView() const
{
    glm::dmat4x4 view{
            {Rotation * axis::X, 0},
            {Rotation * axis::Y, 0},
            {Rotation * axis::Z, 0},
            {0, 0, 0, 1} //pretend that the camera is located at 0,0,0 and translate manually in the vertex shader
    };
    view = glm::inverse(view);

    glm::dmat4x4 vulkan{  //goes from world space (+X = right, +Y = up +Z = forward) to vulkan space (+X = right -Y = up, +Z = forward)
            {1, 0, 0, 0},
            {0, 0, 1, 0},
            {0, -1, 0, 0},
            {0, 0, 0, 1}
    };

    return vulkan * view;
}

glm::dmat4x4 CameraComponent::MakeProjection(int32_t width, int32_t height) const
{
    double AspectRatio = double(width) / double(height);

    glm::dmat4x4 projection{ //maps depth 1 to 0
            {(1.0 / AspectRatio) / std::tan(FieldOfView / 2.0), 0, 0, 0},
            {0, 1.0 / std::tan(FieldOfView / 2.0), 0, 0},
            {0, 0, Near / (Near - Far), 1},
            {0, 0, -((Far * Near) / (Near - Far)), 0}
    };

    return projection;
}

glm::dmat4x4 CameraComponent::MakeViewProjection(int32_t width, int32_t height) const
{
    return MakeProjection(width, height) * MakeView();
}
