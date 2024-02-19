#ifndef STARSIGHT_VK_DESCRIPTOR_HPP
#define STARSIGHT_VK_DESCRIPTOR_HPP

#include "core/ssovector.hpp"
#include <vulkan/vulkan.hpp>
#include <vector>
#include <unordered_map>
#include <mutex>

class VContext;

class VDescriptorAllocator
{
public:

    VDescriptorAllocator(VContext* Context_)
    {
        Context = Context_;
    }

    ~VDescriptorAllocator();

    void reset();

    vk::DescriptorPool grab_pool();
    vk::DescriptorSet allocate_set(vk::DescriptorSetLayout layout, uint32_t variable_descriptor_count);
public:
    VContext* Context = nullptr;
    vk::DescriptorPool current_pool = nullptr;

    std::vector<vk::DescriptorPool> used_pools{};
    std::vector<vk::DescriptorPool> free_pools{};
};

class VDescriptorLayoutCache
{
public:
    struct layout_info_t
    {
        friend bool operator==(const layout_info_t& lhs, const layout_info_t& rhs);

        std::vector<vk::DescriptorSetLayoutBinding> bindings;
        std::vector<vk::DescriptorBindingFlags> flags;
    };

    struct layout_info_hasher
    {
        size_t operator()(const layout_info_t& layout_info) const;
    };

public:

    VDescriptorLayoutCache(VContext* Context_)
    {
        Context = Context_;
    }

    ~VDescriptorLayoutCache();

    vk::DescriptorSetLayout create_layout(layout_info_t& info);
public:

    VContext* Context = nullptr;
    std::unordered_map<layout_info_t, vk::DescriptorSetLayout, layout_info_hasher> layouts{};
};

// When you make the VkDescriptorSetLayoutBinding for your variable length array, the `descriptorCount` member is the maximum number of descriptors in that array.
struct VDescriptorBindInfo
{
    constexpr VDescriptorBindInfo& setBinding(uint32_t binding_) { binding = binding_; return *this;}
    constexpr VDescriptorBindInfo& setCount(uint32_t count_) { count = count_; return *this;}
    constexpr VDescriptorBindInfo& setType(vk::DescriptorType type_) { type = type_; return *this;}
    constexpr VDescriptorBindInfo& setStage(vk::ShaderStageFlags stage_) { stage = stage_; return *this;}
    constexpr VDescriptorBindInfo& setFlags(vk::DescriptorBindingFlags flags_) { flags = flags_; return *this;}

    uint32_t binding = -1;
    uint32_t count = 1;
    vk::DescriptorType type = {};
    vk::ShaderStageFlags stage = {};
    vk::DescriptorBindingFlags flags = {};
};

class VDescriptorBuilder
{
public:
    static inline std::mutex DescriptorBuildMutex{};

    VDescriptorBuilder() = delete;
    VDescriptorBuilder(VDescriptorAllocator* allocator_, VDescriptorLayoutCache* DescriptorLayoutCache_)
            : DescriptorAllocator(allocator_)
            , DescriptorLayoutCache(DescriptorLayoutCache_)
    {
    }

    VDescriptorBuilder& BindSamplers(VDescriptorBindInfo bind_info, const vk::Sampler* samplers);
    VDescriptorBuilder& BindBuffers(VDescriptorBindInfo bind_info, const vk::DescriptorBufferInfo* buffer_info); //buffer info is an optonal array of bindinfo.count size to write
    VDescriptorBuilder& BindImages(VDescriptorBindInfo bind_info, const vk::DescriptorImageInfo* image_info);//image info is an optonal array of bindinfo.count size to write

    void Build(vk::DescriptorSet& out_set, vk::DescriptorSetLayout& out_layout, std::string debug_name = "");
public:

    VDescriptorAllocator* DescriptorAllocator = nullptr;
    VDescriptorLayoutCache* DescriptorLayoutCache = nullptr;

    ssovector<vk::WriteDescriptorSet, 6> writes{};
    VDescriptorLayoutCache::layout_info_t layout_info{};
};

#endif //STARSIGHT_VK_DESCRIPTOR_HPP
