#ifndef STARSIGHT_VK_BUFFER_HPP
#define STARSIGHT_VK_BUFFER_HPP

#include <bit>
#include <functional>
#include <vulkan/vulkan.hpp>
#include "vk_utility.hpp"
#include "vk_memory_allocator.hpp"
#include "concurrentqueue.h"
#include "core/assertion.hpp"

namespace vka
{
    class AllocatedBuffer
    {
    public:
        vk::Device Device = nullptr;
        vma::Allocator Allocator = nullptr;
        moodycamel::ConcurrentQueue<std::function<void()>>& DeferredDestructionQueue;
        vk::Buffer Buffer = nullptr;
        vma::Allocation Allocation = nullptr;
        vma::AllocationCreateFlags AllocationFlags = {};
        vk::BufferUsageFlags BufferUsage = {};
        vma::MemoryUsage MemoryUsage = {};
        uint64_t TotalSize = 0;
        uint64_t UsedSize = 0;
    public:

        ~AllocatedBuffer()
        {
            if(Buffer != nullptr)
            {
                DeferredDestructionQueue.enqueue([Allocator = Allocator, Buffer = Buffer, Allocation = Allocation](){
                    Allocator.destroyBuffer(Buffer, Allocation);
                });
            }
        }

        vma::AllocationInfo GetInfo() const
        {
            return Allocator.getAllocationInfo(Allocation);
        }

        template<typename T = void>
        T* MappedData(uint64_t Offset = 0)
        {
            ASSERT(AllocationFlags & vma::AllocationCreateFlagBits::eMapped && Offset < TotalSize);
            return std::bit_cast<T*>(std::bit_cast<uint64_t>(GetInfo().pMappedData) + Offset);
        }

        vk::DeviceAddress DeviceAddress() const
        {
            ASSERT(BufferUsage & vk::BufferUsageFlagBits::eShaderDeviceAddress);
            return Device.getBufferAddress(vk::BufferDeviceAddressInfo{}.setBuffer(Buffer));
        }

        template<typename T>
        void EmplaceBack(T&& Object)
        {
            uint64_t NewUsedSize = UsedSize + sizeof(T);
            if(NewUsedSize > TotalSize) [[unlikely]]
            {
                Resize(std::max(UsedSize + std::max(sizeof(T), 4096ul), UsedSize + (UsedSize / 2ul)));
            }

            new(MappedData(UsedSize)) T{std::forward<T>(Object)};
            UsedSize = NewUsedSize;
        }

        void Resize(uint64_t NewSize);
    };
}

#endif //STARSIGHT_VK_BUFFER_HPP
