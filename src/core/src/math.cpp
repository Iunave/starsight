#include "math.hpp"

// Returns ±1
static glm::vec2 sign_not_zero(glm::vec2 v)
{
    return {(v.x >= 0.0) ? +1.0 : -1.0, (v.y >= 0.0) ? +1.0 : -1.0};
}

glm::vec2 math::oct_encode(glm::vec3 v)
{
    //Project the sphere onto the octahedron, and then onto the xy plane
    glm::vec2 p = glm::vec2(v.x, v.y) * (1.0f / (std::abs(v.x) + std::abs(v.y) + std::abs(v.z)));
    // Reflect the folds of the lower hemisphere over the diagonals
    return (v.z <= 0.f) ? ((1.0f - glm::vec2(std::abs(p.y), std::abs(p.x))) * sign_not_zero(p)) : p;
}

glm::vec3 math::oct_decode(glm::vec2 e)
{
    glm::vec3 v = glm::vec3(e.x, e.y, 1.0 - abs(e.x) - abs(e.y));
    if(v.z < 0.f)
    {
        glm::vec2 vxy = (1.0f - glm::vec2(abs(v.y), abs(v.x))) * sign_not_zero(glm::vec2(v.x, v.y));
        v.x = vxy.x;
        v.y = vxy.y;
    }
    return normalize(v);
}

glm::vec3 math::hsv2rgb(glm::vec3 c)
{
    glm::vec4 K = glm::vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    glm::vec3 p = abs(fract(c.xxx + K.xyz) * glm::vec3(6.0) - K.www);
    return glm::vec3(c.z) * glm::mix(glm::vec3(K.xxx), glm::clamp(p - K.xxx, glm::vec3(0.0), glm::vec3(1.0)), c.y);
}

glm::vec3 math::rgb2hsv(glm::vec3 c)
{
    glm::vec4 K = glm::vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    glm::vec4 p = glm::mix(glm::vec4(c.bg, K.wz), glm::vec4(c.gb, K.xy), glm::step(c.b, c.g));
    glm::vec4 q = glm::mix(glm::vec4(p.xyw, c.r), glm::vec4(c.r, p.yzx), glm::step(p.x, c.r));

    float d = q.x - std::min(q.w, q.y);
    float e = 1.0e-10;
    return glm::vec3(std::abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

glm::dvec3 math::RandVector(glm::dvec3 min, glm::dvec3 max)
{
    return {
            RandRange(min.x, max.x),
            RandRange(min.y, max.y),
            RandRange(min.z, max.z)
    };
}

glm::dvec3 math::MidPoint(glm::dvec3 A, glm::dvec3 B)
{
    return {
            (A.x + B.x) / 2.0,
            (A.y + B.y) / 2.0,
            (A.z + B.z) / 2.0
    };
}
