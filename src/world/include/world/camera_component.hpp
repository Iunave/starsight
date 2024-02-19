#ifndef STARSIGHT_CAMERA_COMPONENT_HPP
#define STARSIGHT_CAMERA_COMPONENT_HPP

#include "core/math.hpp"

class CameraComponent
{
public:

    enum CameraMode : uint8_t
    {
        fly,
        orbit,
    };

    glm::dvec3 Location;
    glm::dquat Rotation;
    double FieldOfView;
    double Near;
    double Far;
    double MoveSpeed;
    double LookSpeed;
    double RollSpeed;
    bool LockRoll;
    CameraMode Mode = fly;

public:
    CameraComponent();

    void Rotate(double Angle, glm::dvec3 Axis);

    glm::dmat4x4 MakeView() const;
    glm::dmat4x4 MakeProjection(int32_t width, int32_t height) const;
    glm::dmat4x4 MakeViewProjection(int32_t width, int32_t height) const;
};

#endif //STARSIGHT_CAMERA_COMPONENT_HPP
