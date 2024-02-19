#ifndef STARSIGHT_TIME_HPP
#define STARSIGHT_TIME_HPP

#include <cstdint>
#include <ctime>

timespec string2timespec(const char* string);
timespec framerate2frametime(double framerate);
double timespec2double(timespec time);
timespec double2timespec(double time);
timespec timespec_time_now();
double double_time_now();
void time_sleep(timespec time);

timespec operator+(timespec lhs, timespec rhs);
timespec operator*(timespec lhs, double scalar);

inline timespec& operator+=(timespec& lhs, timespec rhs)
{
    lhs = lhs + rhs;
    return lhs;
}

inline timespec operator-(timespec lhs, timespec rhs)
{
    return lhs + timespec{-rhs.tv_sec, -rhs.tv_nsec};
}

inline timespec& operator-=(timespec& lhs, timespec rhs)
{
    lhs = lhs - rhs;
    return lhs;
}

inline timespec& operator*=(timespec& lhs, double scalar)
{
    lhs = lhs * scalar;
    return lhs;
}

class program_time_t
{
public:
    //timespec MinFrameTime{0, 13333333};
    timespec MinFrameTime{0, 0};
    timespec FrameStart{0, 0};
    timespec FrameEnd{0, 0};
    timespec WorkStart{0, 0};
    timespec WorkEnd{0, 0};
    timespec TotalTime{0, 0};
    timespec DeltaTime{0, 0};

    double FloatDelta = 0.0;
    double TimeDilation = 1.0;
    uint64_t FrameCount = 0;

    void StartFrame();
    void EndFrame();
};

namespace global
{
    inline constinit program_time_t ProgramTime{};
}

#endif //STARSIGHT_TIME_HPP
