#include "vk_pipeline.hpp"
#include "core/log.hpp"
#include "core/utility_functions.hpp"
#include "vk_context.hpp"
#include "core/filesystem.hpp"
#include "core/assertion.hpp"
#include "vk_shader.hpp"
#include "shaderc/shaderc.hpp"
#include "vk_descriptor.hpp"

vk::PipelineLayout VPipelineLayoutCache::create_layout(const layout_info_t& layout_info)
{
    auto found_layout = layouts.find(layout_info);

    if(found_layout != layouts.end())
    {
        return found_layout->second;
    }
    else
    {
        LOG_INFO("creating cached pipeline layout [{}]", layouts.size());

        layout_info_t LayoutInfoCopy = layout_info;
        LayoutInfoCopy.set_layouts.shrink_to_fit();
        LayoutInfoCopy.push_constants.shrink_to_fit();

        auto info = vk::PipelineLayoutCreateInfo{}
                .setSetLayouts(LayoutInfoCopy.set_layouts)
                .setPushConstantRanges(LayoutInfoCopy.push_constants);

        vk::PipelineLayout new_layout = Context->Device.createPipelineLayout(info);

        layouts.insert(std::make_pair(std::move(LayoutInfoCopy), new_layout));
        Context->NameObject(new_layout, fmt::format("cached pipeline layout [{}]", layouts.size() - 1));

        return new_layout;
    }
}

template<typename T>
static bool all_exist_in(std::span<T> all, std::span<T> in)
{
    for(const T& lhs : all)
    {
        bool match = false;
        for(const T& rhs : in)
        {
            if(lhs == rhs)
            {
                match = true;
                break;
            }
        }

        if(!match)
        {
            return false;
        }
    }

    return true;
}

bool operator==(const VPipelineLayoutCache::layout_info_t& lhs, const VPipelineLayoutCache::layout_info_t& rhs)
{
    if(lhs.set_layouts.size() != rhs.set_layouts.size() || lhs.push_constants.size() != rhs.push_constants.size())
    {
        return false;
    }

    return all_exist_in(std::span{lhs.set_layouts}, std::span{rhs.set_layouts})
           && all_exist_in(std::span{lhs.push_constants}, std::span{rhs.push_constants});
}

VPipelineLayoutCache::VPipelineLayoutCache(VContext* Context_)
{
    Context = Context_;
    pipeline_cache_path = ProjectAbsolutePath("saved/pipeline_cache_data");

    PipelineCache = global::TaskExecutor.async([this]()
    {
        LOG_INFO("reading pipeline cache from {}", pipeline_cache_path);
        std::vector<uint8_t> PipelineData = ReadFileBinary(pipeline_cache_path, true);

        auto PipelineCacheInfo = vk::PipelineCacheCreateInfo{}
        .setPInitialData(PipelineData.data())
        .setInitialDataSize(PipelineData.size());

        vk::PipelineCache cache = Context->Device.createPipelineCache(PipelineCacheInfo);
        Context->NameObject(cache, "Pipeline Cache");

        return cache;
    });
}

VPipelineLayoutCache::~VPipelineLayoutCache()
{
    LOG_INFO("saving pipeline cache to {}", pipeline_cache_path);

    WriteFileBinary(pipeline_cache_path, Context->Device.getPipelineCacheData(GetPipelineCache()));
    Context->Device.destroyPipelineCache(GetPipelineCache());

    for(auto& pair : layouts)
    {
        Context->Device.destroyPipelineLayout(pair.second);
    }
}

vk::PipelineCache VPipelineLayoutCache::GetPipelineCache()
{
    return PipelineCache.get();
}

size_t VPipelineLayoutCache::layout_info_hasher::operator()(const VPipelineLayoutCache::layout_info_t& layout_info) const
{
    uint64_t setlayouts_hash = hash_crc64(layout_info.set_layouts.data(), layout_info.set_layouts.size() * sizeof(vk::DescriptorSetLayout));
    uint64_t pushconstants_hash = hash_crc64(layout_info.push_constants.data(), layout_info.push_constants.size() * sizeof(vk::PushConstantRange));
    return setlayouts_hash ^ pushconstants_hash;
}

void PipelineBuilder::IncludeShader(std::string ShaderName, vk::SpecializationInfo* Specialization)
{
    Shaders.emplace_back(VDeferredShader{
            .Specialization = Specialization,
            .Shader = ShaderCache->CreateShader(std::move(ShaderName))
    });
}

void VComputePipelineBuilder::Build(vk::PipelineLayout* out_layout, vk::Pipeline* out_pipeline, std::string debug_name)
{
    LOG_INFO("building {} compute pipeline", debug_name.empty() ? "unknown" : debug_name);

    std::vector<vk::PipelineShaderStageCreateInfo> ShaderCreateInfos{};
    VPipelineLayoutCache::layout_info_t PipelineLayoutInfo{};

    for(VDeferredShader& DeferredShader : Shaders)
    {
        VShader* CachedShader = DeferredShader.Shader.get();
        const SpvReflectShaderModule* ReflectionModule = &CachedShader->ReflectionModule;

        auto ShaderStage = static_cast<vk::ShaderStageFlagBits>(ReflectionModule->shader_stage);

        uint32_t PushConstantBlocksCount = 0;
        spvResultCheck = spvReflectEnumeratePushConstantBlocks(ReflectionModule, &PushConstantBlocksCount, nullptr);

        std::vector<SpvReflectBlockVariable*> PushConstantBlocks(PushConstantBlocksCount);
        spvResultCheck = spvReflectEnumeratePushConstantBlocks(ReflectionModule, &PushConstantBlocksCount, PushConstantBlocks.data());

        for(SpvReflectBlockVariable* ReflectedPC : PushConstantBlocks)
        {
            PipelineLayoutInfo.push_constants.emplace_back()
                    .setStageFlags(ShaderStage)
                    .setSize(ReflectedPC->size)
                    .setOffset(ReflectedPC->offset);
        }

        uint32_t DescriptorSetCount = 0;
        spvResultCheck = spvReflectEnumerateDescriptorSets(ReflectionModule, &DescriptorSetCount, nullptr);

        std::vector<SpvReflectDescriptorSet*> DescriptorSets(DescriptorSetCount);
        spvResultCheck = spvReflectEnumerateDescriptorSets(ReflectionModule, &DescriptorSetCount, DescriptorSets.data());

        for(SpvReflectDescriptorSet* ReflectedDS : DescriptorSets)
        {
            VDescriptorLayoutCache::layout_info_t DescriptorLayoutInfo{};

            for(uint32_t binding_idx = 0; binding_idx < ReflectedDS->binding_count; ++binding_idx)
            {
                const SpvReflectDescriptorBinding* ReflectedBinding = ReflectedDS->bindings[binding_idx];

                DescriptorLayoutInfo.bindings.emplace_back()
                        .setStageFlags(ShaderStage)
                        .setBinding(ReflectedBinding->binding)
                        .setDescriptorCount(ReflectedBinding->count)
                        .setDescriptorType(static_cast<vk::DescriptorType>(ReflectedBinding->descriptor_type));

                DescriptorLayoutInfo.flags.emplace_back(); //todo add support for flags?
            }

            vk::DescriptorSetLayout SetLayout = DescriptorLayoutCache->create_layout(DescriptorLayoutInfo);
            PipelineLayoutInfo.set_layouts.emplace_back(SetLayout);
        }

        ShaderCreateInfos.emplace_back()
                .setStage(ShaderStage)
                .setModule(CachedShader->ShaderModule)
                .setPName("main")
                .setPSpecializationInfo(DeferredShader.Specialization);
    }

    *out_layout = PipelineLayoutCache->create_layout(PipelineLayoutInfo);

    auto pipeline_info = vk::ComputePipelineCreateInfo{}
            .setStage(ShaderCreateInfos[0])
            .setLayout(*out_layout);

    vkResultCheck = PipelineLayoutCache->Context->Device.createComputePipelines(PipelineLayoutCache->GetPipelineCache(), 1, &pipeline_info, nullptr, out_pipeline);

    if(!debug_name.empty())
    {
        PipelineLayoutCache->Context->NameObject(*out_pipeline, fmt::format("{} compute pipeline", debug_name));
    }
}

void VGraphicsPipelineBuilder::Build(vk::PipelineLayout* out_layout, vk::Pipeline* out_pipeline, std::string debug_name)
{
    LOG_INFO("building {} graphics pipeline", debug_name.empty() ? "unknown" : debug_name);

    std::vector<vk::PipelineShaderStageCreateInfo> ShaderCreateInfos{};
    VPipelineLayoutCache::layout_info_t PipelineLayoutInfo{};

    for(VDeferredShader& DeferredShader : Shaders)
    {
        DeferredShader.Shader.wait();

        VShader* CachedShader = DeferredShader.Shader.get();
        const SpvReflectShaderModule* ReflectionModule = &CachedShader->ReflectionModule;

        auto ShaderStage = static_cast<vk::ShaderStageFlagBits>(ReflectionModule->shader_stage);

        uint32_t PushConstantBlocksCount = 0;
        spvResultCheck = spvReflectEnumeratePushConstantBlocks(ReflectionModule, &PushConstantBlocksCount, nullptr);

        std::vector<SpvReflectBlockVariable*> PushConstantBlocks(PushConstantBlocksCount);
        spvResultCheck = spvReflectEnumeratePushConstantBlocks(ReflectionModule, &PushConstantBlocksCount, PushConstantBlocks.data());

        for(SpvReflectBlockVariable* ReflectedPC : PushConstantBlocks)
        {
            PipelineLayoutInfo.push_constants.emplace_back()
                    .setStageFlags(ShaderStage)
                    .setSize(ReflectedPC->size)
                    .setOffset(ReflectedPC->offset);
        }

        uint32_t DescriptorSetCount = 0;
        spvResultCheck = spvReflectEnumerateDescriptorSets(ReflectionModule, &DescriptorSetCount, nullptr);

        std::vector<SpvReflectDescriptorSet*> DescriptorSets(DescriptorSetCount);
        spvResultCheck = spvReflectEnumerateDescriptorSets(ReflectionModule, &DescriptorSetCount, DescriptorSets.data());

        for(SpvReflectDescriptorSet* ReflectedDS : DescriptorSets)
        {
            VDescriptorLayoutCache::layout_info_t DescriptorLayoutInfo{};

            for(uint32_t binding_idx = 0; binding_idx < ReflectedDS->binding_count; ++binding_idx)
            {
                const SpvReflectDescriptorBinding* ReflectedBinding = ReflectedDS->bindings[binding_idx];

                auto DescriptorType = static_cast<vk::DescriptorType>(ReflectedBinding->descriptor_type);
                if(DescriptorType == vk::DescriptorType::eUniformBuffer)
                {
                    DescriptorType = vk::DescriptorType::eUniformBufferDynamic; //hacky workaround
                }

                vk::DescriptorBindingFlags DescriptorFlags{};

                uint32_t DescriptorCount;
                if(ReflectedBinding->array.dims_count != 0)
                {
                    ASSERT(ReflectedBinding->array.dims_count == 1);

                    if(ReflectedBinding->array.dims[0] == 0 || ReflectedBinding->array.dims[0] == 1) //unacessed array has dimension of 1....
                    {
                        DescriptorCount = PipelineLayoutCache->Context->FindFreeList(DescriptorType)->PoolSize.descriptorCount;
                        DescriptorFlags |= vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind;
                    }
                    else
                    {
                        DescriptorCount = ReflectedBinding->array.dims[0];
                    }
                }
                else
                {
                    DescriptorCount = ReflectedBinding->count;
                }

                DescriptorLayoutInfo.bindings.emplace_back()
                        .setStageFlags(ShaderStage)
                        .setBinding(ReflectedBinding->binding)
                        .setDescriptorCount(DescriptorCount)
                        .setDescriptorType(DescriptorType);

                DescriptorLayoutInfo.flags.emplace_back(DescriptorFlags); //todo add support for flags?
            }

            vk::DescriptorSetLayout SetLayout = DescriptorLayoutCache->create_layout(DescriptorLayoutInfo);
            PipelineLayoutInfo.set_layouts.emplace_back(SetLayout);
        }

        ShaderCreateInfos.emplace_back()
                .setStage(ShaderStage)
                .setModule(CachedShader->ShaderModule)
                .setPName("main")
                .setPSpecializationInfo(DeferredShader.Specialization);
    }

    *out_layout = PipelineLayoutCache->create_layout(PipelineLayoutInfo);

    auto pipeline_info = vk::GraphicsPipelineCreateInfo{}
            .setPNext(&rendering)
            .setLayout(*out_layout)
            .setStages(ShaderCreateInfos)
            .setPDynamicState(&dynamic_state)
            .setPVertexInputState(&vertex_input)
            .setPInputAssemblyState(&input_assembly)
            .setPViewportState(&viewport)
            .setPRasterizationState(&rasterization)
            .setPMultisampleState(&multisample)
            .setPColorBlendState(&color_blend)
            .setPDepthStencilState(&depth_stencil);

    vkResultCheck = PipelineLayoutCache->Context->Device.createGraphicsPipelines(PipelineLayoutCache->GetPipelineCache(), 1, &pipeline_info, nullptr, out_pipeline);

    if(!debug_name.empty())
    {
        PipelineLayoutCache->Context->NameObject(*out_pipeline, debug_name);
    }
}

vk::PipelineDynamicStateCreateInfo pipeline_helpers::dynamic_viewport_scissor()
{
    static std::array states{vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    return vk::PipelineDynamicStateCreateInfo{{}, states};
}

vk::PipelineVertexInputStateCreateInfo pipeline_helpers::make_vertex_input(const vertex_input_t& input)
{
    return vk::PipelineVertexInputStateCreateInfo{input.flags, input.bindings, input.attributes};
}

vk::PipelineInputAssemblyStateCreateInfo pipeline_helpers::topology(vk::PrimitiveTopology primitive_topology)
{
    return vk::PipelineInputAssemblyStateCreateInfo{{}, primitive_topology, false};
}

vk::PipelineRasterizationStateCreateInfo pipeline_helpers::rasterization(vk::CullModeFlagBits cullmode, vk::PolygonMode polygonmode)
{
    return vk::PipelineRasterizationStateCreateInfo{}
            .setFrontFace(vk::FrontFace::eCounterClockwise)
            .setCullMode(cullmode)
            .setPolygonMode(polygonmode)
            .setRasterizerDiscardEnable(false)
            .setDepthClampEnable(false)
            .setDepthBiasEnable(false)
            .setLineWidth(1.0);
}

vk::PipelineMultisampleStateCreateInfo pipeline_helpers::no_multisample()
{
    return vk::PipelineMultisampleStateCreateInfo{}
            .setSampleShadingEnable(false)
            .setMinSampleShading(1.0f)
            .setRasterizationSamples(vk::SampleCountFlagBits::e1)
            .setAlphaToCoverageEnable(false)
            .setAlphaToOneEnable(false);
}

vk::PipelineDepthStencilStateCreateInfo pipeline_helpers::reversed_depth()
{
    return vk::PipelineDepthStencilStateCreateInfo{}
            .setDepthTestEnable(true)
            .setDepthWriteEnable(true)
            .setDepthCompareOp(vk::CompareOp::eGreater)
            .setDepthBoundsTestEnable(true)
            .setMinDepthBounds(0.0)
            .setMaxDepthBounds(1.0)
            .setStencilTestEnable(false);
}

vk::PipelineDepthStencilStateCreateInfo pipeline_helpers::normal_depth()
{
    return vk::PipelineDepthStencilStateCreateInfo{}
            .setDepthTestEnable(true)
            .setDepthWriteEnable(true)
            .setDepthCompareOp(vk::CompareOp::eLess)
            .setDepthBoundsTestEnable(false)
            .setMinDepthBounds(0.0)
            .setMaxDepthBounds(1.0)
            .setStencilTestEnable(false);
}

vk::PipelineViewportStateCreateInfo pipeline_helpers::single_viewport_scissor()
{
    return vk::PipelineViewportStateCreateInfo{}
            .setViewportCount(1)
            .setScissorCount(1);
}

vk::PipelineDepthStencilStateCreateInfo pipeline_helpers::no_depth()
{
    return vk::PipelineDepthStencilStateCreateInfo{}
            .setDepthTestEnable(false)
            .setDepthWriteEnable(false)
            .setDepthCompareOp(vk::CompareOp::eLess)
            .setDepthBoundsTestEnable(false)
            .setMinDepthBounds(0.0)
            .setMaxDepthBounds(1.0)
            .setStencilTestEnable(false);
}
