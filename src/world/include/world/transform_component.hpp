#ifndef STARSIGHT_TRANSFORM_COMPONENT_HPP
#define STARSIGHT_TRANSFORM_COMPONENT_HPP

#include "core/math.hpp"

class TransformComponent
{
public:
    glm::dvec3 location{0,0,0};
    glm::fquat rotation = glm::identity<glm::dquat>();
    glm::fvec3 scale{1,1,1};
};

#endif //STARSIGHT_TRANSFORM_COMPONENT_HPP
