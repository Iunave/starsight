#include "world.hpp"
#include "fmt/format.h"
#include "core/log.hpp"
#include "input_module.hpp"
#include "audio_module.hpp"
#include "render_module.hpp"
#include <string>

/* Logging function. The level should be interpreted as: */
/* >0: Debug tracing. Only enabled in debug builds. */
/*  0: Tracing. Enabled in debug/release builds. */
/* -2: Warning. An issue occurred, but operation was successful. */
/* -3: Error. An issue occurred, and operation was unsuccessful. */
/* -4: Fatal. An issue occurred, and application must quit. */
void FlecsLog(int32_t level, const char *file, int32_t line, const char *msg)
{
    if(level > 0)
    {
        LOG_DEBUG("{}", msg);
    }
    else if(level == 0)
    {
        LOG_INFO("{}", msg);
    }
    else if(level == -2)
    {
        LOG_WARNING("{}", msg);
    }
    else if(level == -3)
    {
        LOG_ERROR("{}", msg);
    }
    else if(level == -4)
    {
        LOG_CRITICAL("{}", msg);
    }
}

std::optional<flecs::world> CreateWorld()
{
    ecs_os_set_api_defaults();

    ecs_os_api_t ecsOsApi = ecs_os_get_api();
    ecsOsApi.log_ = &::FlecsLog;
#ifndef NDEBUG
    ecsOsApi.log_level_ = 0;
#else
    ecsOsApi.log_level_ = 2;
#endif
    ecs_os_set_api(&ecsOsApi);

    flecs::world World{};
    World.set_threads(std::max(1u, std::thread::hardware_concurrency()));
    World.import<InputModule>();
    World.import<RenderModule>();
    World.import<AudioModule>();

    return World;
}