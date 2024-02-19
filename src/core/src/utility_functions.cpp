#include "utility_functions.hpp"

bool InMainThread()
{
    ASSERT(global::MainThreadID != 0);
    return pthread_equal(global::MainThreadID, pthread_self());
}