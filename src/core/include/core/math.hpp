#ifndef STARSIGHT_MATH_HPP
#define STARSIGHT_MATH_HPP

#define GLM_FORCE_INTRINSICS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_SWIZZLE
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtc/type_ptr.hpp"

#include <cmath>

//assembly functions
extern "C" double rand_normal();
extern "C" double randrange_flt(double min, double max);
extern "C" int64_t randrange_int(int64_t min, int64_t max);
extern "C" uint32_t count_mipmaps(uint32_t width, uint32_t height);
extern "C" uint64_t hash_crc32(const void* data, uint64_t bytes);
extern "C" uint64_t hash_crc64(const void* data, uint64_t bytes);

namespace axis
{
    inline const glm::dvec3 X{1.0, 0.0, 0.0};
    inline const glm::dvec3 Y{0.0, 1.0, 0.0};
    inline const glm::dvec3 Z{0.0, 0.0, 1.0};

    inline const glm::dvec3 left = -X;
    inline const glm::dvec3 right = X;

    inline const glm::dvec3 forward = Y;
    inline const glm::dvec3 backward = -Y;

    inline const glm::dvec3 up = Z;
    inline const glm::dvec3 down = -Z;
}

namespace math
{
    glm::vec2 oct_encode(glm::vec3 n);
    glm::vec3 oct_decode(glm::vec2 f);

    glm::vec3 hsv2rgb(glm::vec3 c);
    glm::vec3 rgb2hsv(glm::vec3 c);

    glm::dvec3 RandVector(glm::dvec3 min, glm::dvec3 max);
    glm::dvec3 MidPoint(glm::dvec3 A, glm::dvec3 B);

    template<std::integral T>
    constexpr T iPow(T base, T exponent)
    {
        T result = 1;
        while(true)
        {
            if(exponent & 1)
            {
                result *= base;
            }

            exponent >>= 1;

            if(!exponent)
            {
                break;
            }

            base *= base;
        }

        return result;
    }

    inline glm::dvec4 NormalizePlane(glm::dvec4 Plane)
    {
        return Plane / glm::length(glm::dvec3{Plane.xyz});
    }

    inline constexpr uint64_t PadSize2Alignment(uint64_t Size, uint64_t Alignment)
    {
        return (Size + Alignment - 1) & ~(Alignment - 1);
    }

    template<typename T>
    inline T frac(T val)
    {
        T whole = std::floor(val);
        return val - whole;
    }

    template<std::integral T>
    T pack_snorm(float val)
    {
        return std::round(val * float(std::numeric_limits<T>::max()));
    }

    template<std::integral T>
    float unpack_snorm(T val)
    {
        return float(val) / std::numeric_limits<T>::max();
    }

    inline double RandRange(double min, double max)
    {
        return randrange_flt(min, max);
    }

    inline int64_t RandRange(int64_t min, int64_t max)
    {
        return randrange_int(min, max);
    }
}

#endif //STARSIGHT_MATH_HPP
