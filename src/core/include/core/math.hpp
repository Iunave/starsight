#ifndef STARSIGHT_MATH_HPP
#define STARSIGHT_MATH_HPP

#define GLM_FORCE_INTRINSICS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#define GLM_FORCE_SWIZZLE
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtc/type_ptr.hpp"

//assembly functions
extern "C" double rand_normal();
extern "C" double randrange_flt(double min, double max);
extern "C" int64_t randrange_int(int64_t min, int64_t max);
extern "C" uint32_t count_mipmaps(uint32_t width, uint32_t height);
extern "C" uint64_t hash_crc32(const void* data, uint64_t bytes);
extern "C" uint64_t hash_crc64(const void* data, uint64_t bytes);

namespace axis
{
    inline const glm::vec3 X{1.0, 0.0, 0.0};
    inline const glm::vec3 Y{0.0, 1.0, 0.0};
    inline const glm::vec3 Z{0.0, 0.0, 1.0};

    inline const glm::vec3 left = -X;
    inline const glm::vec3 right = X;

    inline const glm::vec3 forward = Y;
    inline const glm::vec3 backward = -Y;

    inline const glm::vec3 up = Z;
    inline const glm::vec3 down = -Z;
}

namespace math
{
    double RandRange(double min, double max)
    {
        return randrange_flt(min, max);
    }

    int64_t RandRange(int64_t min, int64_t max)
    {
        return randrange_int(min, max);
    }
}

#endif //STARSIGHT_MATH_HPP
