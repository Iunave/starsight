#ifndef STARSIGHT_UTILITY_FUNCTIONS_HPP
#define STARSIGHT_UTILITY_FUNCTIONS_HPP

#include <pthread.h>
#include "taskflow/taskflow.hpp"
#include "assertion.hpp"

#define SafeFree(ptr)       \
{                           \
    ASSERT(ptr != nullptr); \
    free(ptr);              \
    ptr = nullptr;          \
}

#define SafeDelete(ptr)     \
{                           \
    ASSERT(ptr != nullptr); \
    delete ptr;             \
    ptr = nullptr;          \
}

#define TYPE_ALLOCA(type, count) static_cast<type*>(__builtin_alloca(sizeof(type) * count));

namespace global
{
    inline tf::Executor TaskExecutor{};
    inline pthread_t MainThreadID = 0;
}

bool InMainThread();

#endif //STARSIGHT_UTILITY_FUNCTIONS_HPP
