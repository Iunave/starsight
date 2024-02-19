#include "vk_descriptor.hpp"
#include "core/log.hpp"
#include "vk_context.hpp"
#include "core/assertion.hpp"
#include "core/utility_functions.hpp"

using enum vk::DescriptorType;
constexpr std::array descriptor_pool_sizes //configure this
{
        vk::DescriptorPoolSize{eCombinedImageSampler, 1000},
        vk::DescriptorPoolSize{eSampler, 1000},
        vk::DescriptorPoolSize{eSampledImage, 1000}
};


namespace descriptor_tracker
{
    constexpr std::array<vk::DescriptorType, 17> types
    {
            vk::DescriptorType::eSampler,
            vk::DescriptorType::eCombinedImageSampler,
            vk::DescriptorType::eSampledImage,
            vk::DescriptorType::eStorageImage,
            vk::DescriptorType::eUniformTexelBuffer,
            vk::DescriptorType::eStorageTexelBuffer,
            vk::DescriptorType::eUniformBuffer,
            vk::DescriptorType::eStorageBuffer,
            vk::DescriptorType::eUniformBufferDynamic,
            vk::DescriptorType::eStorageBufferDynamic,
            vk::DescriptorType::eInputAttachment,
            vk::DescriptorType::eInlineUniformBlock,
            vk::DescriptorType::eAccelerationStructureKHR,
            vk::DescriptorType::eAccelerationStructureNV,
            vk::DescriptorType::eMutableVALVE,
            vk::DescriptorType::eSampleWeightImageQCOM,
            vk::DescriptorType::eBlockMatchImageQCOM
    };

    std::array<uint64_t, 17> allocated_descriptors{};
    uint64_t pool_count = 0;

    uint64_t& allocated(vk::DescriptorType type)
    {
        switch(type)
        {
            case vk::DescriptorType::eSampler: return allocated_descriptors[0];
            case vk::DescriptorType::eCombinedImageSampler: return allocated_descriptors[1];
            case vk::DescriptorType::eSampledImage: return allocated_descriptors[2];
            case vk::DescriptorType::eStorageImage: return allocated_descriptors[3];
            case vk::DescriptorType::eUniformTexelBuffer: return allocated_descriptors[4];
            case vk::DescriptorType::eStorageTexelBuffer: return allocated_descriptors[5];
            case vk::DescriptorType::eUniformBuffer: return allocated_descriptors[6];
            case vk::DescriptorType::eStorageBuffer: return allocated_descriptors[7];
            case vk::DescriptorType::eUniformBufferDynamic: return allocated_descriptors[8];
            case vk::DescriptorType::eStorageBufferDynamic: return allocated_descriptors[9];
            case vk::DescriptorType::eInputAttachment: return allocated_descriptors[10];
            case vk::DescriptorType::eInlineUniformBlock: return allocated_descriptors[11];
            case vk::DescriptorType::eAccelerationStructureKHR: return allocated_descriptors[12];
            case vk::DescriptorType::eAccelerationStructureNV: return allocated_descriptors[13];
            case vk::DescriptorType::eMutableVALVE: return allocated_descriptors[14];
            case vk::DescriptorType::eSampleWeightImageQCOM: return allocated_descriptors[15];
            case vk::DescriptorType::eBlockMatchImageQCOM: return allocated_descriptors[16];
        }
    }

    uint64_t reserved(vk::DescriptorType type)
    {
        for(vk::DescriptorPoolSize pool_size : descriptor_pool_sizes)
        {
            if(pool_size.type == type)
            {
                return pool_size.descriptorCount * pool_count;
            }
        }
        return 0;
    }

    void track(vk::DescriptorType type, uint64_t count)
    {
        allocated(type) += count;
    }

    void untrack(vk::DescriptorType type, uint64_t count)
    {
        allocated(type) -= count;
    }

    void report_usage(vk::DescriptorType type)
    {
        uint64_t reserved_descriptors = reserved(type);
        uint64_t used_descriptors = allocated(type);
        double part = reserved_descriptors != 0 ? (double(used_descriptors) / double(reserved_descriptors)) : 1.0;

        LOG_INFO("{}: {} / {} ({})", vk::to_string(type), used_descriptors, reserved_descriptors, part);
    }

    void report_usage()
    {
        LOG_INFO("");
        LOG_INFO("descriptor usage statistics");

        for(vk::DescriptorType type : types)
        {
            report_usage(type);
        }

        LOG_INFO("");
    }

    void reset()
    {
        memset(allocated_descriptors.data(), 0, allocated_descriptors.size() * sizeof(uint64_t));
        pool_count = 0;
    }
};

vk::DescriptorPool VDescriptorAllocator::grab_pool()
{
    vk::DescriptorPool pool;

    if(!free_pools.empty())
    {
        pool = free_pools.back();
        free_pools.pop_back();
    }
    else
    {
        LOG_INFO("allocating new descriptor pool");

        auto pool_info = vk::DescriptorPoolCreateInfo{}
                .setFlags(vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind)
                .setPoolSizes(descriptor_pool_sizes)
                .setMaxSets(1000);

        pool = Context->Device.createDescriptorPool(pool_info);
    }

    Context->NameObject(pool, fmt::format("descriptor pool in use [{}]", used_pools.size()));
    descriptor_tracker::pool_count += 1;

    return pool;
}

vk::DescriptorSet VDescriptorAllocator::allocate_set(vk::DescriptorSetLayout layout, uint32_t variable_descriptor_count)
{
    vk::DescriptorSet set = nullptr;

    if(!current_pool)
    {
        current_pool = grab_pool();
        used_pools.push_back(current_pool);
    }

    auto variable_count = vk::DescriptorSetVariableDescriptorCountAllocateInfo{}
            .setDescriptorSetCount(1)
            .setDescriptorCounts(variable_descriptor_count);

    auto alloc_info = vk::DescriptorSetAllocateInfo{}
            .setPNext(&variable_count)
            .setDescriptorPool(current_pool)
            .setDescriptorSetCount(1)
            .setSetLayouts(layout);

    vk::Result alloc_result = Context->Device.allocateDescriptorSets(&alloc_info, &set);

    if(alloc_result == vk::Result::eErrorFragmentedPool || alloc_result == vk::Result::eErrorOutOfPoolMemory)
    {
        current_pool = grab_pool();
        used_pools.push_back(current_pool);

        alloc_info.setDescriptorPool(current_pool);

        vkResultCheck = Context->Device.allocateDescriptorSets(&alloc_info, &set);
    }
    else
    {
        vkResultCheck = alloc_result;
    }

    return set;
}

void VDescriptorAllocator::reset()
{
    for(uint64_t index = 0; index < used_pools.size(); ++index)
    {
        Context->Device.resetDescriptorPool(used_pools[index]);
        free_pools.emplace_back(used_pools[index]);

        Context->NameObject(used_pools[index], fmt::format("free descriptor pool [{}]", free_pools.size() - 1));
    }

    used_pools.clear();
    current_pool = nullptr;
}

VDescriptorAllocator::~VDescriptorAllocator()
{
    descriptor_tracker::report_usage();

    for(vk::DescriptorPool pool : used_pools)
    {
        Context->Device.destroyDescriptorPool(pool);
    }
    used_pools.clear();

    for(vk::DescriptorPool pool : free_pools)
    {
        Context->Device.destroyDescriptorPool(pool);
    }
    free_pools.clear();
}

vk::DescriptorSetLayout VDescriptorLayoutCache::create_layout(layout_info_t& info)
{
    ASSERT(info.bindings.size() == info.flags.size());

    auto* ascending_bindings = TYPE_ALLOCA(vk::DescriptorSetLayoutBinding, info.bindings.size());
    auto* ascending_flags = TYPE_ALLOCA(vk::DescriptorBindingFlags, info.flags.size());

    for(size_t index = 0; index < info.bindings.size(); ++index) //inserts the bindings in ascending order
    {
        uint32_t binding = info.bindings[index].binding;

        ASSERT(binding < info.bindings.size());

        ascending_bindings[binding] = info.bindings[index];
        ascending_flags[binding] = info.flags[index];
    }

    memcpy(info.bindings.data(), ascending_bindings, sizeof(vk::DescriptorSetLayoutBinding) * info.bindings.size());
    memcpy(info.flags.data(), ascending_flags, sizeof(vk::DescriptorBindingFlags) * info.flags.size());

    auto found_layout = layouts.find(info);
    if(found_layout != layouts.end())
    {
        return found_layout->second;
    }
    else
    {
        LOG_INFO("creating cached descriptor layout [{}]", layouts.size());

        layout_info_t LayoutInfoCopy = info;
        LayoutInfoCopy.bindings.shrink_to_fit();
        LayoutInfoCopy.flags.shrink_to_fit();

        auto binding_flags = vk::DescriptorSetLayoutBindingFlagsCreateInfo{}
                .setBindingFlags(LayoutInfoCopy.flags);

        auto layout_info = vk::DescriptorSetLayoutCreateInfo{}
                .setPNext(&binding_flags)
                .setBindings(LayoutInfoCopy.bindings);

        if(info.flags.back() & vk::DescriptorBindingFlagBits::eUpdateAfterBind)
        {
            layout_info.setFlags(vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool);
        }

        vk::DescriptorSetLayout new_layout = Context->Device.createDescriptorSetLayout(layout_info);

        layouts.insert(std::make_pair(std::move(LayoutInfoCopy), new_layout));
        Context->NameObject(new_layout, fmt::format("cached descriptor layout [{}]", layouts.size() - 1));

        return new_layout;
    }
}

bool operator==(const VDescriptorLayoutCache::layout_info_t& lhs, const VDescriptorLayoutCache::layout_info_t& rhs)
{
    if(lhs.bindings.size() != rhs.bindings.size())
    {
        return false;
    }

    for(uint64_t binding = 0; binding < lhs.bindings.size(); ++binding) //bindings are ASSUMED sorted by ascending binding so they match
    {
        if(lhs.bindings[binding].descriptorType != rhs.bindings[binding].descriptorType
           || lhs.bindings[binding].descriptorCount != rhs.bindings[binding].descriptorCount
           || lhs.bindings[binding].stageFlags != rhs.bindings[binding].stageFlags
           || lhs.flags[binding] != rhs.flags[binding])
        {
            return false;
        }
    }

    return true;
}

VDescriptorLayoutCache::~VDescriptorLayoutCache()
{
    for(auto& pair : layouts)
    {
        Context->Device.destroyDescriptorSetLayout(pair.second);
    }
    layouts.clear();
}

size_t VDescriptorLayoutCache::layout_info_hasher::operator()(const layout_info_t& layout_info) const
{
    uint64_t bindings_hash = hash_crc64(layout_info.bindings.data(), layout_info.bindings.size() * sizeof(vk::DescriptorSetLayoutBinding));
    uint64_t flags_hash = hash_crc64(layout_info.flags.data(), layout_info.flags.size() * sizeof(vk::DescriptorBindingFlags));
    return bindings_hash ^ flags_hash;
}

VDescriptorBuilder& VDescriptorBuilder::BindSamplers(VDescriptorBindInfo bind_info, const vk::Sampler* samplers)
{
    layout_info.bindings.emplace_back()
            .setBinding(bind_info.binding)
            .setDescriptorCount(bind_info.count)
            .setDescriptorType(bind_info.type)
            .setStageFlags(bind_info.stage)
            .setPImmutableSamplers(samplers);

    layout_info.flags.emplace_back(bind_info.flags);

    return *this;
}

VDescriptorBuilder& VDescriptorBuilder::BindBuffers(VDescriptorBindInfo bind_info, const vk::DescriptorBufferInfo* buffer_info)
{
    layout_info.bindings.emplace_back()
            .setBinding(bind_info.binding)
            .setDescriptorCount(bind_info.count)
            .setDescriptorType(bind_info.type)
            .setStageFlags(bind_info.stage);

    layout_info.flags.emplace_back(bind_info.flags);

    if(buffer_info)
    {
        writes.emplace_back()
                .setDescriptorCount(bind_info.count)
                .setDescriptorType(bind_info.type)
                .setDstBinding(bind_info.binding)
                .setDstArrayElement(0)
                .setPBufferInfo(buffer_info);
    }

    return *this;
}

VDescriptorBuilder& VDescriptorBuilder::BindImages(VDescriptorBindInfo bind_info, const vk::DescriptorImageInfo* image_info)
{
    layout_info.bindings.emplace_back()
            .setBinding(bind_info.binding)
            .setDescriptorCount(bind_info.count)
            .setDescriptorType(bind_info.type)
            .setStageFlags(bind_info.stage);

    layout_info.flags.emplace_back(bind_info.flags);

    if(image_info)
    {
        writes.emplace_back()
                .setDescriptorCount(bind_info.count)
                .setDescriptorType(bind_info.type)
                .setDstBinding(bind_info.binding)
                .setDstArrayElement(0)
                .setPImageInfo(image_info);
    }

    return *this;
}

void VDescriptorBuilder::Build(vk::DescriptorSet& out_set, vk::DescriptorSetLayout& out_layout, std::string debug_name)
{
    LOG_INFO("building {} descriptor", debug_name.empty() ? "unspecified" : debug_name);

    ASSERT(!layout_info.bindings.empty());


    for(const vk::DescriptorSetLayoutBinding& outer : layout_info.bindings)
    {
        for(const vk::DescriptorSetLayoutBinding& inner : layout_info.bindings)
        {
            ASSERT((&outer != &inner) == (outer.binding != inner.binding), "duplicate bindings found");
        }
    }

    DescriptorBuildMutex.lock();

    out_layout = DescriptorLayoutCache->create_layout(layout_info);
    out_set = DescriptorAllocator->allocate_set(out_layout, layout_info.bindings.back().descriptorCount);

    for(const vk::DescriptorSetLayoutBinding& binding : layout_info.bindings)
    {
        descriptor_tracker::track(binding.descriptorType, binding.descriptorCount); //todo
    }

    DescriptorBuildMutex.unlock();

    if(!debug_name.empty())
    {
        DescriptorAllocator->Context->NameObject(out_set, fmt::format("{} descriptor set", debug_name));
    }

    if(!writes.empty())
    {
        for(vk::WriteDescriptorSet& write : writes)
        {
            write.setDstSet(out_set);
        }

        DescriptorAllocator->Context->Device.updateDescriptorSets(writes, {});
    }
}