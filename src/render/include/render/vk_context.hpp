#ifndef STARSIGHT_VK_CONTEXT_HPP
#define STARSIGHT_VK_CONTEXT_HPP

#include "vk_memory_allocator.hpp"
#include "vk_utility.hpp"

#include <bit>
#include <vulkan/vulkan.hpp>
#include <cstdint>
#include <functional>
#include <vector>
#include <span>
#include <mutex>

struct VPhysicalDeviceFeatures
{
    VPhysicalDeviceFeatures()
    {
        vk13features.pNext = nullptr;
        vk12features.pNext = &vk13features;
        vk11features.pNext = &vk12features;
        features2.pNext = &vk11features;
    }

    vk::PhysicalDeviceVulkan13Features vk13features{};
    vk::PhysicalDeviceVulkan12Features vk12features{};
    vk::PhysicalDeviceVulkan11Features vk11features{};
    vk::PhysicalDeviceFeatures2 features2{};
};

struct VQueueIndices
{
    uint32_t Graphics = UINT32_MAX;
    uint32_t Present = UINT32_MAX;
    uint32_t Transfer = UINT32_MAX;
    uint32_t Compute = UINT32_MAX;
};

struct VQueueHandles
{
    vk::Queue Graphics = nullptr;
    vk::Queue Present = nullptr;
    vk::Queue Transfer = nullptr;
    vk::Queue Compute = nullptr;
};

class VContext
{
public:

    std::vector<std::function<void()>> DestructionQueue{};

    std::vector<const char*> ValidationLayers{};
    std::vector<const char*> InstanceExtensions{};
    std::vector<const char*> DeviceExtensions{};

    vk::Instance Instance = nullptr;
    vk::DebugUtilsMessengerEXT DebugMessenger = nullptr;
    VPhysicalDeviceFeatures PhysicalDeviceFeatures{};
    vk::PhysicalDeviceProperties2 PhysicalDeviceProperties{};
    VQueueIndices QueueIndices{};
    VQueueHandles QueueHandles{};
    vk::PhysicalDevice PhysicalDevice = nullptr;
    vk::Device Device = nullptr;
    vma::Allocator Allocator = nullptr;
    vk::CommandPool GraphicsCommandPool = nullptr;
    vk::CommandPool TransferCommandPool = nullptr;
    std::mutex TransferMutex{};
/*
    VDescriptorAllocator DescriptorAllocator{};
    VDescriptorLayoutCache DescriptorLayoutCache{};
    VPipelineLayoutCache PipelineLayoutCache{};
    VShaderCache ShaderModuleCache{};

    VTransferManager TransferManager{};
*/
public:

    VContext();
    ~VContext();

    void NameObject(uint64_t Handle, vk::ObjectType Type, std::string Name);

    template<typename T>
    void NameObject(T Object, std::string Name)
    {
        NameObject(std::bit_cast<uint64_t>(Object), T::objectType, std::move(Name));
    }

    vk::CommandBuffer RequestTransferCommandBuffer(std::string Description = "");
    void SubmitTransferCommands(vk::CommandBuffer Commands, vk::SubmitInfo2 Submits);
    void FreeTransferCommandBuffer(vk::CommandBuffer Commands);

    vk::Format PickImageFormat(vk::FormatFeatureFlags feature_flags, std::span<vk::Format> candidates) const;
    vk::Format PickBufferFormat(vk::FormatFeatureFlags feature_flags, std::span<vk::Format> candidates) const;
/*
    VGraphicsPipelineBuilder MakeGraphicsPipelineBuilder();
    VDescriptorBuilder MakeDescriptorBuilder();
*/
    VAllocatedBuffer AllocateStagingBuffer(uint64_t Size);
    VAllocatedBuffer AllocateVertexBuffer(uint64_t Size);

private:

    void CreateInstance();
    void CreateDebugMessenger();
    void PickPhysicalDevice();
    void CreateLogicalDevice();
    void CreateMemoryAllocator();
    void CreateCommandPool();
    void CreateCaches();
};

inline VContext* vkContext = nullptr;

#endif //STARSIGHT_VK_CONTEXT_HPP
