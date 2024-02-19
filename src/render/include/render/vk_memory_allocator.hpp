#ifndef STARSIGHT_VK_MEMORY_ALLOCATOR_HPP
#define STARSIGHT_VK_MEMORY_ALLOCATOR_HPP

#ifndef NDEBUG
/*
#include "core/log.hpp"
#define VMA_DEBUG_LOG(...) LOG_DEBUG(__VA_ARGS__)
*/
#endif

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"

#include "VulkanMemoryAllocator/include/vk_mem_alloc.h"
#include "VulkanMemoryAllocator-Hpp/include/vk_mem_alloc.hpp"

#pragma clang diagnostic pop

#endif //STARSIGHT_VK_MEMORY_ALLOCATOR_HPP
