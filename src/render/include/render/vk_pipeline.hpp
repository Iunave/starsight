#ifndef STARSIGHT_VK_PIPELINE_HPP
#define STARSIGHT_VK_PIPELINE_HPP

#include "core/ssovector.hpp"
#include "core/filesystem.hpp"
#include <vulkan/vulkan.hpp>
#include <unordered_map>
#include <future>

class VContext;
struct VShader;
class VShaderCache;
class VDescriptorLayoutCache;

struct vertex_input_t
{
    std::vector<vk::VertexInputBindingDescription> bindings;
    std::vector<vk::VertexInputAttributeDescription> attributes;
    vk::PipelineVertexInputStateCreateFlagBits flags = {};
};

class VPipelineLayoutCache
{
public:
    struct layout_info_t
    {
        friend bool operator==(const layout_info_t& lhs, const layout_info_t& rhs);

        std::vector<vk::DescriptorSetLayout> set_layouts;
        std::vector<vk::PushConstantRange> push_constants;
    };

    struct layout_info_hasher
    {
        size_t operator()(const layout_info_t& layout_info) const;
    };

public:

    explicit VPipelineLayoutCache(VContext* Context_);
    ~VPipelineLayoutCache();

    vk::PipelineCache GetPipelineCache();

    vk::PipelineLayout create_layout(const layout_info_t& layout_info);

    VContext* Context;
private:

    std::fpath pipeline_cache_path{};
    std::shared_future<vk::PipelineCache> PipelineCache{};

    std::unordered_map<layout_info_t, vk::PipelineLayout, layout_info_hasher> layouts{};
};

struct PipelineBuilder
{
protected:
    struct VDeferredShader
    {
        vk::SpecializationInfo* Specialization;
        std::future<VShader*> Shader;
    };

    ssovector<VDeferredShader, 5> Shaders{};

    VDescriptorLayoutCache* DescriptorLayoutCache = nullptr;
    VPipelineLayoutCache* PipelineLayoutCache = nullptr;
    VShaderCache* ShaderCache = nullptr;
public:

    PipelineBuilder(VDescriptorLayoutCache* DescriptorLayoutCache_, VPipelineLayoutCache* PipelineLayoutCache_, VShaderCache* ShaderCache_)
            : DescriptorLayoutCache(DescriptorLayoutCache_)
            , PipelineLayoutCache(PipelineLayoutCache_)
            , ShaderCache(ShaderCache_)
    {
    }

    void IncludeShader(std::string ShaderName, vk::SpecializationInfo* Specialization = nullptr);
    virtual void Build(vk::PipelineLayout* out_layout, vk::Pipeline* out_pipeline, std::string debug_name) = 0;
};

struct VComputePipelineBuilder : public PipelineBuilder
{
    using PipelineBuilder::PipelineBuilder;
    void Build(vk::PipelineLayout* out_layout, vk::Pipeline* out_pipeline, std::string debug_name) override;
};

struct VGraphicsPipelineBuilder : public PipelineBuilder
{
    vk::PipelineDynamicStateCreateInfo dynamic_state{};
    vk::PipelineRenderingCreateInfo rendering{};
    vk::PipelineVertexInputStateCreateInfo vertex_input{};
    vk::PipelineInputAssemblyStateCreateInfo input_assembly{};
    vk::PipelineViewportStateCreateInfo viewport{};
    vk::PipelineRasterizationStateCreateInfo rasterization{};
    vk::PipelineMultisampleStateCreateInfo multisample{};
    vk::PipelineColorBlendStateCreateInfo color_blend{};
    vk::PipelineDepthStencilStateCreateInfo depth_stencil{};

    using PipelineBuilder::PipelineBuilder;
    void Build(vk::PipelineLayout* out_layout, vk::Pipeline* out_pipeline, std::string debug_name) override;
};

namespace pipeline_helpers
{
    vk::PipelineDynamicStateCreateInfo dynamic_viewport_scissor();
    vk::PipelineViewportStateCreateInfo single_viewport_scissor();
    vk::PipelineVertexInputStateCreateInfo make_vertex_input(const vertex_input_t& input);
    vk::PipelineInputAssemblyStateCreateInfo topology(vk::PrimitiveTopology primitive_topology);
    vk::PipelineRasterizationStateCreateInfo rasterization(vk::CullModeFlagBits cullmode, vk::PolygonMode polygonmode = vk::PolygonMode::eFill);
    vk::PipelineMultisampleStateCreateInfo no_multisample();
    vk::PipelineDepthStencilStateCreateInfo no_depth();
    vk::PipelineDepthStencilStateCreateInfo reversed_depth();
    vk::PipelineDepthStencilStateCreateInfo normal_depth();

    template<size_t color_attachments>
    vk::PipelineColorBlendStateCreateInfo no_colorblend()
    {
        static std::array<vk::PipelineColorBlendAttachmentState, color_attachments> blend_states;

        auto expand_tuple = []<std::size_t... I>(std::index_sequence<I...>)
        {
            using enum vk::ColorComponentFlagBits;
            (blend_states[I].setBlendEnable(false).setColorWriteMask(eR | eG | eB | eA), ...);
        };
        expand_tuple(std::make_index_sequence<color_attachments>{});

        return vk::PipelineColorBlendStateCreateInfo{}
                .setLogicOpEnable(false)
                .setAttachments(blend_states);
    }
}

#endif //STARSIGHT_VK_PIPELINE_HPP
