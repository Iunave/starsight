#ifndef STARSIGHT_VK_CONTEXT_HPP
#define STARSIGHT_VK_CONTEXT_HPP

#include "vk_memory_allocator.hpp"
#include "vk_utility.hpp"
#include "vk_model.hpp"
#include "vk_descriptor.hpp"
#include "vk_pipeline.hpp"
#include "vk_shader.hpp"
#include "concurrentqueue.h"

#include <vulkan/vulkan.hpp>
#include <bit>
#include <cstdint>
#include <functional>
#include <vector>
#include <span>
#include <mutex>

#ifndef UNIFORM_BUFFER_ALIGNMENT
#define UNIFORM_BUFFER_ALIGNMENT 256
#endif

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

struct DescriptorSetFreeList
{
    std::list<uint32_t> FreeList{};
    std::mutex ListMX{};
    vk::DescriptorPoolSize PoolSize{};
    uint32_t LastSlot = 0;
};

class VContext
{
public:
    std::vector<std::function<void()>> DestructionQueue{};
    moodycamel::ConcurrentQueue<std::function<void()>> DeferredDestructionQueue{};

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
    vk::DescriptorPool ShaderResourcePool = nullptr;
    vk::DescriptorSetLayout ShaderResourceLayout = nullptr;
    vk::DescriptorSet ShaderResourceSet = nullptr;
    std::array<DescriptorSetFreeList, 1> DescriptorSetFreeLists{};
    vk::CommandPool GraphicsCommandPool = nullptr;
    vk::CommandPool TransferCommandPool = nullptr;
    std::mutex TransferMutex{};
    vk::DescriptorPool TransientDescriptorPool = nullptr;

    VAllocatedBuffer GlobalIndexBuffer{};
    std::vector<BufferAllocationSlot> IndexBufferAllocationList{};
    std::mutex IndexBufferMx{};

    VAllocatedBuffer GlobalVertexBuffer{};
    std::vector<BufferAllocationSlot> VertexBufferAllocationList{};
    std::mutex VertexBufferMx{};

    std::optional<VDescriptorAllocator> DescriptorAllocator;
    std::optional<VDescriptorLayoutCache> DescriptorLayoutCache;
    std::optional<VPipelineLayoutCache> PipelineLayoutCache;
    std::optional<VShaderCache> ShaderModuleCache;
    std::optional<VModelManager> ModelManager;

    const class CameraComponent* Camera = nullptr;

    struct BaseInitializer
    {
        std::vector<const char*> ValidationLayers;
        std::vector<const char*> InstanceExtensions;
        std::vector<const char*> DeviceExtensions;
        VPhysicalDeviceFeatures PhysicalDeviceFeatures;
    };

public:

    VContext(const BaseInitializer& Initializer);
    virtual ~VContext();

    void NameObject(uint64_t Handle, vk::ObjectType Type, const std::string& Name);

    template<typename T>
    void NameObject(T Object, std::string Name)
    {
        NameObject(std::bit_cast<uint64_t>(Object), T::objectType, std::move(Name));
    }

    uint64_t PadUniformSize(uint64_t Size) const;
    vk::DeviceSize GetBufferAddress(vk::Buffer Buffer) const;

    vk::Format PickImageFormat(vk::FormatFeatureFlags feature_flags, std::span<vk::Format> candidates) const;
    vk::Format PickBufferFormat(vk::FormatFeatureFlags feature_flags, std::span<vk::Format> candidates) const;

    vk::CommandBuffer RequestTransferCommandBuffer(std::string Description = "");
    void SubmitTransferCommands(vk::CommandBuffer Commands, vk::SubmitInfo2 Submits);
    void FreeTransferCommandBuffer(vk::CommandBuffer Commands);

    VGraphicsPipelineBuilder MakeGraphicsPipelineBuilder();
    VComputePipelineBuilder MakeComputePipelineBuilder();
    VDescriptorBuilder MakeDescriptorBuilder();

    VAllocatedBuffer AllocateBuffer(uint64_t Size, vk::BufferUsageFlags BufferUsage, vma::AllocationCreateFlags AllocationFlags, vma::MemoryUsage MemoryUsage, const std::string& BufferAllocationName = "");
    void ReallocateBuffer(VAllocatedBuffer* Buffer, uint64_t NewSize);
    void FreeBuffer(VAllocatedBuffer* Buffer);

    DescriptorSetFreeList* FindFreeList(vk::DescriptorType Type);
    uint32_t GrabDescriptorSlot(vk::DescriptorType Type);
    void FreeDescriptorSlot(vk::DescriptorType Type, uint32_t Slot);

    BufferAllocationSlot GrabIndexBufferMemory(uint32_t Size, uint32_t Alignment);
    void FreeIndexBufferMemory(BufferAllocationSlot Slot);

    BufferAllocationSlot GrabVertexBufferMemory(uint32_t Size, uint32_t Alignment);
    void FreeVertexBufferMemory(BufferAllocationSlot Slot);

private:

    void CreateInstance();
    void CreateDebugMessenger();
    void PickPhysicalDevice();
    void CreateLogicalDevice();
    void CreateMemoryAllocator();
    void CreateCommandPool();
    void CreateShaderDescriptorTable();
    void CreateTransientDescriptorPool();
    void AllocateGlobalIndexAndVertexBuffer();
};

inline VContext* vkContext = nullptr;

#endif //STARSIGHT_VK_CONTEXT_HPP
