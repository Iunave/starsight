#include "time.hpp"
#include "assertion.hpp"
#include "math.hpp"
#include <cerrno>
#include <ratio>

static uint64_t parse_seconds(const char* string, uint64_t* out_index)
{
    uint64_t index = 0;
    uint64_t seconds = 0;

    while(string[index] != '\0' && string[index] != '.')
    {
        char character = string[index];
        ASSERT(character >= '0' && character <= '9');

        seconds *= 10;
        seconds += (character - '0');
        ++index;
    }

    if(out_index)
    {
        *out_index = index;
    }

    return seconds;
}

static uint64_t parse_nanoseconds(const char* string, uint64_t* out_index)
{
    uint64_t index = 0;
    uint64_t nanoseconds = 0;
    uint64_t decimal_value = std::nano::den / 10;

    while(string[index] != '\0')
    {
        char character = string[index];
        ASSERT(character >= '0' && character <= '9');

        nanoseconds += (character - '0') * decimal_value;
        decimal_value /= 10;
        ++index;
    }

    if(out_index)
    {
        *out_index = index;
    }

    return nanoseconds;
}

void program_time_t::StartFrame()
{
    WorkStart = timespec_time_now();
    FrameStart = WorkStart;
}

void program_time_t::EndFrame()
{
    FrameCount += 1;

    WorkEnd = timespec_time_now();
    timespec work_delta = WorkEnd - WorkStart;
    timespec undershoot = MinFrameTime - work_delta;

    if(undershoot.tv_sec >= 0 && undershoot.tv_nsec > 0)
    {
        VERIFY(nanosleep(&undershoot, nullptr) != -1 || errno == EINTR);
    }

    FrameEnd = timespec_time_now();
    DeltaTime = FrameEnd - FrameStart;
    DeltaTime *= TimeDilation;
    TotalTime +=  DeltaTime;
    FloatDelta = timespec2double(DeltaTime);
}

timespec string2timespec(const char* string)
{
    uint64_t index = 0;
    uint64_t seconds = parse_seconds(string, &index);
    uint64_t nanoseconds = 0;

    if(string[index] == '.')
    {
        nanoseconds = parse_nanoseconds(string + index + 1, nullptr);
    }

    return {__time_t(seconds), __time_t(nanoseconds)};
}

timespec framerate2frametime(double framerate)
{
    return double2timespec(1.0 / framerate);
}

double timespec2double(timespec time)
{
    return double(time.tv_sec) + (double(time.tv_nsec) / double(std::nano::den));
}

timespec double2timespec(double time)
{
    int64_t seconds = std::trunc(time);
    int64_t nanoseconds = std::round(math::frac(time) * std::nano::den);

    return timespec{seconds, nanoseconds};
}

double double_time_now()
{
    return timespec2double(timespec_time_now());
}

timespec timespec_time_now()
{
    timespec res{};
    ASSERT(clock_gettime(CLOCK_MONOTONIC, &res) != -1);
    return res;
}

void time_sleep(timespec time)
{
    ASSERT(nanosleep(&time, nullptr) != -1);
}

timespec operator+(timespec lhs, timespec rhs)
{
    lhs.tv_sec += rhs.tv_sec;
    lhs.tv_nsec += rhs.tv_nsec;

    if(lhs.tv_nsec < 0)
    {
        lhs.tv_sec -= 1;
        lhs.tv_nsec += std::nano::den;
    }
    else if(lhs.tv_nsec >= std::nano::den)
    {
        lhs.tv_sec += 1;
        lhs.tv_nsec -= std::nano::den;
    }

    return lhs;
}

///@warning multiplying with a negative might produce funny effects!
timespec operator*(timespec lhs, double scalar)
{
    static_assert(std::is_signed_v<__time_t> && std::is_signed_v<__syscall_slong_t> && std::is_signed_v<decltype(std::nano::den)>);

    double sec = double(lhs.tv_sec) * scalar;
    double nsec = double(lhs.tv_nsec) * scalar;
    double sec_whole = std::floor(sec);
    double sec_frac = sec - sec_whole;
    double nsec_discarded = sec_frac * double(std::nano::den);

    lhs.tv_sec = __time_t(sec_whole);
    lhs.tv_nsec = __syscall_slong_t(std::round(nsec) + nsec_discarded);

    //todo can we come up with a smarter solution here...
    while(lhs.tv_nsec < 0)
    {
        lhs.tv_sec -= 1;
        lhs.tv_nsec += std::nano::den;
    }

    while(lhs.tv_nsec >= std::nano::den)
    {
        lhs.tv_sec += 1;
        lhs.tv_nsec -= std::nano::den;
    }

    return lhs;
}
