#ifndef STARSIGHT_VK_UTILITY_HPP
#define STARSIGHT_VK_UTILITY_HPP

#include <vulkan/vulkan.hpp>
#include "vk_memory_allocator.hpp"

#include <ratio>

class result_checker_t
{
public:
    result_checker_t& operator=(vk::Result val);
    result_checker_t& operator=(VkResult val);

    operator vk::Result() const {return Value;}

    vk::Result Value = vk::Result::eSuccess;
};
inline constinit thread_local result_checker_t vkResultCheck{};

struct VAllocatedBuffer
{
    vk::Buffer Buffer = nullptr;
    vma::Allocation Allocation = nullptr;
    vk::DeviceAddress BufferAddress = 0;
    void* MappedData = nullptr;
    uint64_t Size = 0;
    vk::BufferUsageFlags BufferUsage = {};
    vma::AllocationCreateFlags AllocationFlags = {};
    vma::MemoryUsage MemoryUsage = {};
};

struct VAllocatedImage
{
    vk::Image Image = nullptr;
    vk::ImageView ImageView = nullptr;
    vma::Allocation Allocation = nullptr;
    vma::AllocationInfo Info{};
};

struct BufferAllocationSlot
{
    uint32_t Offset;
    uint32_t Size;
};

namespace vkutil
{
    inline static constexpr uint64_t default_timeout = std::nano::den * 10; //10 seconds
    inline static constexpr std::array<float, 4> default_label_color{0, 0, 0, 1};

    void push_label(vk::Queue queue, std::string label_name, std::array<float, 4> color = default_label_color);
    void insert_label(vk::Queue queue, std::string label_name, std::array<float, 4> color = default_label_color);
    void pop_label(vk::Queue queue);

    void push_label(vk::CommandBuffer cmd, std::string label_name, std::array<float, 4> color = default_label_color);
    void insert_label(vk::CommandBuffer cmd, std::string label_name, std::array<float, 4> color = default_label_color);
    void pop_label(vk::CommandBuffer cmd);

    vk::ImageSubresourceLayers flat_subresource_layers(vk::ImageAspectFlagBits aspect);
    vk::ImageSubresourceRange flat_subresource_range(vk::ImageAspectFlagBits aspect);

    uint32_t GroupCount(uint32_t total_size, uint32_t local_size);
}

#endif //STARSIGHT_VK_UTILITY_HPP
