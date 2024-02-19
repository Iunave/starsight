#include "vk_render_target.hpp"
#include "vk_context.hpp"
#include "window/window.hpp"
#include "core/log.hpp"
#include "core/assertion.hpp"
#include "../../world/include/world/camera_component.hpp"

using PipelineStage = vk::PipelineStageFlagBits2;
using AccessFlag = vk::AccessFlagBits2;

static uint32_t find_surface_format(vk::PhysicalDevice pdevice, vk::SurfaceKHR surface, std::span<vk::SurfaceFormatKHR> candidates)
{
    std::vector<vk::SurfaceFormatKHR> available = pdevice.getSurfaceFormatsKHR(surface);

    for(uint32_t candidate_index = 0; candidate_index < candidates.size(); ++candidate_index)
    {
        for(uint32_t available_index = 0; available_index < available.size(); ++available_index)
        {
            if(available[available_index] == candidates[candidate_index])
            {
                return candidate_index;
            }
        }
    }
    return -1;
}

static uint32_t find_present_mode(vk::PhysicalDevice pdevice, vk::SurfaceKHR surface, std::span<vk::PresentModeKHR> candidates)
{
    std::vector<vk::PresentModeKHR> available = pdevice.getSurfacePresentModesKHR(surface);

    for(uint32_t candidate_index = 0; candidate_index < candidates.size(); ++candidate_index)
    {
        for(uint32_t available_index = 0; available_index < available.size(); ++available_index)
        {
            if(available[available_index] == candidates[candidate_index])
            {
                return candidate_index;
            }
        }
    }
    return -1;
}

static std::optional<vk::SurfaceFormatKHR> FindSurfaceFormat(vk::PhysicalDevice PhysicalDevice, vk::SurfaceKHR Surface)
{
    std::array surface_formats
            {
                    vk::SurfaceFormatKHR{vk::Format::eR8G8B8A8Srgb, vk::ColorSpaceKHR::eSrgbNonlinear},
                    vk::SurfaceFormatKHR{vk::Format::eB8G8R8A8Srgb, vk::ColorSpaceKHR::eSrgbNonlinear},
                    vk::SurfaceFormatKHR{vk::Format::eR8G8B8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear},
                    vk::SurfaceFormatKHR{vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear},
            };

    uint32_t found_format = find_surface_format(PhysicalDevice, Surface, surface_formats);
    if(found_format != -1)
    {
        return surface_formats[found_format];
    }
    else
    {
        return {};
    }
}

static std::optional<vk::PresentModeKHR> FindPresentMode(vk::PhysicalDevice PhysicalDevice, vk::SurfaceKHR Surface)
{
    std::array present_modes
            {
                    vk::PresentModeKHR::eMailbox,
                    vk::PresentModeKHR::eImmediate,
                    vk::PresentModeKHR::eFifoRelaxed,
                    vk::PresentModeKHR::eFifo
            };

    uint32_t found_mode = find_present_mode(PhysicalDevice, Surface, present_modes);
    if(found_mode != -1)
    {
        return present_modes[found_mode];;
    }
    else
    {
        return {};
    }
}

static VContext::BaseInitializer MakeVulkanBaseInitializer(GLFWwindow* Window)
{
    VContext::BaseInitializer Initializer{};

    uint32_t NumInstanceExtensions;
    const char** InstanceExtensions = glfwGetRequiredInstanceExtensions(&NumInstanceExtensions);

    for(uint32_t idx = 0; idx < NumInstanceExtensions; ++idx)
    {
        Initializer.InstanceExtensions.emplace_back(InstanceExtensions[idx]);
    }

    Initializer.DeviceExtensions.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    Initializer.DeviceExtensions.emplace_back(VK_EXT_CONDITIONAL_RENDERING_EXTENSION_NAME);

#ifndef NDEBUG
    Initializer.ValidationLayers.emplace_back("VK_LAYER_KHRONOS_validation");
    Initializer.InstanceExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

    Initializer.PhysicalDeviceFeatures.features2.features.samplerAnisotropy = true;
    Initializer.PhysicalDeviceFeatures.features2.features.shaderInt16 = true;
    Initializer.PhysicalDeviceFeatures.features2.features.shaderInt64 = true;
    Initializer.PhysicalDeviceFeatures.vk11features.shaderDrawParameters = true;
    Initializer.PhysicalDeviceFeatures.vk11features.storageBuffer16BitAccess = true;
    Initializer.PhysicalDeviceFeatures.vk12features.storageBuffer8BitAccess = true;
    Initializer.PhysicalDeviceFeatures.vk12features.runtimeDescriptorArray = true;
    Initializer.PhysicalDeviceFeatures.vk12features.descriptorIndexing = true;
    Initializer.PhysicalDeviceFeatures.vk12features.descriptorBindingPartiallyBound = true;
    Initializer.PhysicalDeviceFeatures.vk12features.descriptorBindingVariableDescriptorCount = true;
    Initializer.PhysicalDeviceFeatures.vk12features.descriptorBindingSampledImageUpdateAfterBind = true;
    Initializer.PhysicalDeviceFeatures.vk12features.drawIndirectCount = true;
    Initializer.PhysicalDeviceFeatures.vk12features.timelineSemaphore = true;
    Initializer.PhysicalDeviceFeatures.vk12features.scalarBlockLayout = true;
    Initializer.PhysicalDeviceFeatures.vk12features.bufferDeviceAddress = true;
    Initializer.PhysicalDeviceFeatures.vk12features.vulkanMemoryModel = true;
    Initializer.PhysicalDeviceFeatures.vk12features.vulkanMemoryModelDeviceScope = true;
    Initializer.PhysicalDeviceFeatures.vk13features.synchronization2 = true;
    Initializer.PhysicalDeviceFeatures.vk13features.dynamicRendering = true;
    Initializer.PhysicalDeviceFeatures.vk13features.maintenance4 = true;

    return Initializer;
}

VStarSightRenderer::VStarSightRenderer(GLFWwindow* Window_)
    : VContext(MakeVulkanBaseInitializer(Window_))
{
    Window = Window_;

    CreateSurface();
    CreateSwapChain(nullptr);

    DestructionQueue.emplace_back([this](){
        Device.destroySwapchainKHR(SwapChain);
    });

    CreateSwapChainViews();

    DestructionQueue.emplace_back([this]()
    {
        for(vk::ImageView view : ImageViews)
        {
            Device.destroyImageView(view);
        }
    });

    CreateFrames();
    ActiveFrame = Frames.begin();

    CreateGBuffer();

    DestructionQueue.emplace_back([this](){
        DestroyGBuffer();
    });

    CreateForwardPipeline();
    CreateGeometryPipeline();
    CreateGlobalLightPipeline();
    CreateDrawCommandsPipeline();
    CreateCameraBuffer();
    CreateIndirectCommandsBuffer();
}

VStarSightRenderer::~VStarSightRenderer()
{
    LOG_INFO("destroying renderer");

    if(Device)
    {
        Device.waitIdle();
    }

    if(ActiveFrame != nullptr)
    {
        for(VFrame& Frame : Frames)
        {
            for(auto& func : Frame.OnFrameBegin)
            {
                func();
            }
        }
    }

    std::function<void()> func;
    while(DeferredDestructionQueue.try_dequeue(func))
    {
        func();
    }

    for(auto it = DestructionQueue.rbegin(); it != DestructionQueue.rend(); ++it)
    {
        (*it)();
    }
}

void VStarSightRenderer::CreateSurface()
{
    LOG_INFO("creating surface for {}", GetWindowName());

    vkResultCheck = glfwCreateWindowSurface(Instance, Window, nullptr, reinterpret_cast<VkSurfaceKHR*>(&Surface));

    DestructionQueue.emplace_back([this]{
        Instance.destroySurfaceKHR(Surface);
    });

    auto Format = FindSurfaceFormat(PhysicalDevice, Surface);
    VERIFY(Format.has_value());

    SurfaceFormat = Format.value();

    auto Mode = FindPresentMode(PhysicalDevice, Surface);
    VERIFY(Mode.has_value());

    PresentMode = Mode.value();
}

void VStarSightRenderer::CreateSwapChain(vk::SwapchainKHR OldSwapChain)
{
    LOG_INFO("creating swapchain");

    vk::SurfaceCapabilitiesKHR surface_capabilities = PhysicalDevice.getSurfaceCapabilitiesKHR(Surface);
    const vk::Extent2D& current_extent = surface_capabilities.currentExtent;

    if(current_extent.width != std::numeric_limits<uint32_t>::max()) //we have to use the capabilities extent
    {
        ImageExtent = current_extent;
    }
    else
    {
        auto[window_width, window_height] = GetWindowExtent();

        const vk::Extent2D& min_extent = surface_capabilities.minImageExtent;
        const vk::Extent2D& max_extent = surface_capabilities.maxImageExtent;

        window_width = std::clamp(uint32_t(window_width), min_extent.width, max_extent.width);
        window_height = std::clamp(uint32_t(window_height), min_extent.height, max_extent.height);

        ImageExtent = vk::Extent2D{uint32_t(window_width), uint32_t(window_height)};
    }

    LOG_INFO("image extent {}x{}", ImageExtent.width, ImageExtent.height);

    MinImageCount = std::max(surface_capabilities.minImageCount, uint32_t(FRAMES_IN_FLIGHT));
    VERIFY(surface_capabilities.maxImageCount == 0 || surface_capabilities.maxImageCount >= MinImageCount);

    SurfaceFormat.format = vk::Format::eB8G8R8A8Unorm;

    auto SwapChainInfo = vk::SwapchainCreateInfoKHR{}
            .setSurface(Surface)
            .setMinImageCount(MinImageCount)
            .setImageFormat(SurfaceFormat.format)
            .setImageColorSpace(SurfaceFormat.colorSpace)
            .setImageExtent(ImageExtent)
            .setImageArrayLayers(1)
            .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eStorage)
            .setPreTransform(surface_capabilities.currentTransform)
            .setPresentMode(PresentMode)
            .setClipped(true) //discards hidden pixels
            .setOldSwapchain(OldSwapChain);

    if(surface_capabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::eOpaque)
    {
        SwapChainInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    }
    else if(surface_capabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::eInherit)
    {
        SwapChainInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eInherit;
    }
    else
    {
        VERIFY(false, "no composite alpha format found");
    }

    uint32_t graphics_queue = QueueIndices.Graphics;
    uint32_t present_queue = QueueIndices.Present;

    const std::array used_queues{graphics_queue, present_queue};

    if(graphics_queue != present_queue)
    {
        SharingMode = vk::SharingMode::eConcurrent;

        SwapChainInfo.queueFamilyIndexCount = used_queues.size();
        SwapChainInfo.pQueueFamilyIndices = used_queues.data();
    }
    else
    {
        SharingMode = vk::SharingMode::eExclusive;
    }

    SwapChainInfo.imageSharingMode = SharingMode;

    SwapChain = Device.createSwapchainKHR(SwapChainInfo);
    NameObject(SwapChain, "swapchain");
}

void VStarSightRenderer::CreateSwapChainViews()
{
    LOG_INFO("creating swapchain images");

    Images = Device.getSwapchainImagesKHR(SwapChain);
    ImageViews.resize(Images.size());

    LOG_INFO("swapchain image count {}", Images.size());

    auto subresource = vk::ImageSubresourceRange{}
            .setAspectMask(vk::ImageAspectFlagBits::eColor)
            .setBaseMipLevel(0)
            .setLevelCount(1)
            .setBaseArrayLayer(0)
            .setLayerCount(1);

    auto view_info = vk::ImageViewCreateInfo{}
            .setFormat(SurfaceFormat.format)
            .setComponents(vk::ComponentSwizzle::eIdentity)
            .setSubresourceRange(subresource)
            .setViewType(vk::ImageViewType::e2D);

    for(uint64_t index = 0; index < Images.size(); ++index)
    {
        NameObject(Images[index], fmt::format("swapchain image [{}]", index));

        view_info.setImage(Images[index]);

        ImageViews[index] = Device.createImageView(view_info);
        NameObject(ImageViews[index], fmt::format("swapchain image view [{}]", index));
    }
}

void VStarSightRenderer::RecreateSwapChain()
{
    WaitForFrames();

    DestroyGBuffer();
    CreateGBuffer();

    for(vk::ImageView view : ImageViews)
    {
        Device.destroyImageView(view);
    }
    ImageViews.clear();

    vk::SwapchainKHR OldSwapChain = SwapChain;
    CreateSwapChain(OldSwapChain);
    Device.destroySwapchainKHR(OldSwapChain);

    CreateSwapChainViews();
}

void VStarSightRenderer::CreateFrames()
{
    LOG_INFO("creating frames");

    auto CommandBufferInfo = vk::CommandBufferAllocateInfo{}
            .setCommandBufferCount(FRAMES_IN_FLIGHT)
            .setCommandPool(GraphicsCommandPool)
            .setLevel(vk::CommandBufferLevel::ePrimary);

    std::array<vk::CommandBuffer, FRAMES_IN_FLIGHT> CommandBuffers{};
    vkResultCheck = Device.allocateCommandBuffers(&CommandBufferInfo, CommandBuffers.data());

    auto FenceCreateInfo = vk::FenceCreateInfo{}
            .setFlags(vk::FenceCreateFlagBits::eSignaled);

    auto SemaphoreCreateInfo = vk::SemaphoreCreateInfo{}
            .setFlags({});

    for(uint64_t frame = 0; frame < Frames.size(); ++frame)
    {
        Frames[frame].CommandBuffer = CommandBuffers[frame];
        Frames[frame].InFlight = Device.createFence(FenceCreateInfo);
        Frames[frame].ImageAvailable = Device.createSemaphore(SemaphoreCreateInfo);
        Frames[frame].DrawFinished = Device.createSemaphore(SemaphoreCreateInfo);

        NameObject(Frames[frame].CommandBuffer, fmt::format("command buffer {} [{}]", GetWindowName(), frame));
        NameObject(Frames[frame].InFlight, fmt::format("in flight {} [{}]", GetWindowName(), frame));
        NameObject(Frames[frame].ImageAvailable, fmt::format("image available {} [{}]", GetWindowName(), frame));
        NameObject(Frames[frame].DrawFinished, fmt::format("draw finished {} [{}]", GetWindowName(), frame));

        DestructionQueue.emplace_back([frame, this](){
            Device.destroy(Frames[frame].InFlight);
            Device.destroy(Frames[frame].ImageAvailable);
            Device.destroy(Frames[frame].DrawFinished);
        });
    }

    LOG_INFO("allocating MeshTransforms");

    for(uint64_t frame = 0; frame < Frames.size(); ++frame)
    {
        VAllocatedBuffer& MeshTransforms = Frames[frame].MeshTransforms;

        uint64_t BufferSize = sizeof(VShaderTransform) * DEVICE_MESH_ALLOCATION_STEP;
        vk::BufferUsageFlags BufferFlags = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress;
        vma::AllocationCreateFlags AllocationFlags = vma::AllocationCreateFlagBits::eMapped | vma::AllocationCreateFlagBits::eHostAccessSequentialWrite | vma::AllocationCreateFlagBits::eStrategyBestFit;
        vma::MemoryUsage MemoryUsage = vma::MemoryUsage::eAutoPreferDevice;

        MeshTransforms = AllocateBuffer(BufferSize, BufferFlags, AllocationFlags, MemoryUsage, fmt::format("MeshTransforms [{}]", frame));

        DestructionQueue.emplace_back([this, MeshTransforms = &MeshTransforms](){
                Allocator.destroyBuffer(MeshTransforms->Buffer, MeshTransforms->Allocation);
        });
    }

    LOG_INFO("allocating MeshBounds");

    for(uint64_t frame = 0; frame < Frames.size(); ++frame)
    {
        uint64_t BufferSize = sizeof(glm::fvec4) * DEVICE_MESH_ALLOCATION_STEP;
        vk::BufferUsageFlags BufferFlags = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress;
        vma::AllocationCreateFlags AllocationFlags = vma::AllocationCreateFlagBits::eMapped | vma::AllocationCreateFlagBits::eHostAccessSequentialWrite | vma::AllocationCreateFlagBits::eStrategyBestFit;
        vma::MemoryUsage MemoryUsage = vma::MemoryUsage::eAutoPreferDevice;

        VAllocatedBuffer& MeshBounds = Frames[frame].MeshBounds;
        MeshBounds = AllocateBuffer(BufferSize, BufferFlags, AllocationFlags, MemoryUsage, fmt::format("MeshBounds [{}]", frame));

        DestructionQueue.emplace_back([this, MeshBounds = &MeshBounds](){
            Allocator.destroyBuffer(MeshBounds->Buffer, MeshBounds->Allocation);
        });
    }

    LOG_INFO("allocating MeshInfos");

    for(uint64_t frame = 0; frame < Frames.size(); ++frame)
    {
        uint64_t BufferSize = sizeof(VShaderMeshInfo) * DEVICE_MESH_ALLOCATION_STEP;
        vk::BufferUsageFlags BufferFlags = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress;
        vma::AllocationCreateFlags AllocationFlags = vma::AllocationCreateFlagBits::eMapped | vma::AllocationCreateFlagBits::eHostAccessSequentialWrite | vma::AllocationCreateFlagBits::eStrategyBestFit;
        vma::MemoryUsage MemoryUsage = vma::MemoryUsage::eAutoPreferDevice;

        VAllocatedBuffer& MeshInfos = Frames[frame].MeshInfos;
        MeshInfos = AllocateBuffer(BufferSize, BufferFlags, AllocationFlags, MemoryUsage, fmt::format("MeshInfos [{}]", frame));

        DestructionQueue.emplace_back([this, MeshInfos = &MeshInfos](){
            Allocator.destroyBuffer(MeshInfos->Buffer, MeshInfos->Allocation);
        });
    }
}

void VStarSightRenderer::CreateDrawCommandsPipeline()
{
    VComputePipelineBuilder Builder = MakeComputePipelineBuilder();
    Builder.IncludeShader(ProjectAbsolutePath("shaders/build_draw_commands.comp"));
    Builder.Build(&BuildDrawCommandsLayout, &BuildDrawCommandsPipeline, "BuildDrawCommands");

    DestructionQueue.emplace_back([this]{
        Device.destroyPipeline(BuildDrawCommandsPipeline);
    });
}

void VStarSightRenderer::CreateForwardPipeline()
{
    VGraphicsPipelineBuilder Builder = MakeGraphicsPipelineBuilder();

    Builder.IncludeShader(ProjectAbsolutePath("shaders/forward.vert"));
    Builder.IncludeShader(ProjectAbsolutePath("shaders/forward.frag"));

    Builder.dynamic_state = pipeline_helpers::dynamic_viewport_scissor();
    Builder.viewport = pipeline_helpers::single_viewport_scissor();
    Builder.rendering.setColorAttachmentFormats(SurfaceFormat.format);
    Builder.rendering.setDepthAttachmentFormat(GBuffer.DepthFormat);
    //Builder.vertex_input;
    Builder.input_assembly = pipeline_helpers::topology(vk::PrimitiveTopology::eTriangleList);
    Builder.rasterization = pipeline_helpers::rasterization(vk::CullModeFlagBits::eBack);
    Builder.multisample = pipeline_helpers::no_multisample();
    Builder.color_blend = pipeline_helpers::no_colorblend<1>();
    Builder.depth_stencil = pipeline_helpers::reversed_depth();

    Builder.Build(&ForwardPipelineLayout, &ForwardPipeline, fmt::format("forward pipeline {}", GetWindowName()));

    DestructionQueue.emplace_back([this](){
        Device.destroyPipeline(ForwardPipeline);
    });
}

void VStarSightRenderer::CreateGBuffer()
{
    LOG_INFO("creating GBuffer");

    std::array PositionFormats{vk::Format::eR32G32B32Sfloat, vk::Format::eR32G32B32A32Sfloat};
    std::array NormalFormats{vk::Format::eR16G16Snorm, vk::Format::eR32G32Sfloat};
    std::array ColorFormats{vk::Format::eR8G8B8A8Srgb};
    std::array DepthFormats{vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint};

    GBuffer.PositionFormat = PickImageFormat(vk::FormatFeatureFlagBits::eColorAttachment | vk::FormatFeatureFlagBits::eSampledImage, PositionFormats);
    GBuffer.NormalFormat = PickImageFormat(vk::FormatFeatureFlagBits::eColorAttachment | vk::FormatFeatureFlagBits::eSampledImage, NormalFormats);
    GBuffer.ColorFormat = PickImageFormat(vk::FormatFeatureFlagBits::eColorAttachment | vk::FormatFeatureFlagBits::eSampledImage, ColorFormats);
    GBuffer.DepthFormat = PickImageFormat(vk::FormatFeatureFlagBits::eDepthStencilAttachment | vk::FormatFeatureFlagBits::eSampledImage, DepthFormats);

    auto[Width, Height] = GetWindowExtent();

    auto PositionCreateInfo = vk::ImageCreateInfo{}
            .setUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled)
            .setExtent(vk::Extent3D{Width, Height, 1})
            .setArrayLayers(1)
            .setMipLevels(1)
            .setFormat(GBuffer.PositionFormat)
            .setImageType(vk::ImageType::e2D)
            .setTiling(vk::ImageTiling::eOptimal)
            .setSamples(vk::SampleCountFlagBits::e1)
            .setInitialLayout(vk::ImageLayout::eUndefined);

    auto PositionAllocateInfo = vma::AllocationCreateInfo{}
            .setFlags(vma::AllocationCreateFlagBits::eStrategyBestFit)
            .setUsage(vma::MemoryUsage::eAutoPreferDevice);

    auto NormalCreateInfo = vk::ImageCreateInfo{}
            .setUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled)
            .setExtent(vk::Extent3D{Width, Height, 1})
            .setArrayLayers(1)
            .setMipLevels(1)
            .setFormat(GBuffer.NormalFormat)
            .setImageType(vk::ImageType::e2D)
            .setTiling(vk::ImageTiling::eOptimal)
            .setSamples(vk::SampleCountFlagBits::e1)
            .setInitialLayout(vk::ImageLayout::eUndefined);

    auto NormalAllocateInfo = vma::AllocationCreateInfo{}
            .setFlags(vma::AllocationCreateFlagBits::eStrategyBestFit)
            .setUsage(vma::MemoryUsage::eAutoPreferDevice);

    auto ColorCreateInfo = vk::ImageCreateInfo{}
            .setUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled)
            .setExtent(vk::Extent3D{Width, Height, 1})
            .setArrayLayers(1)
            .setMipLevels(1)
            .setFormat(GBuffer.ColorFormat)
            .setImageType(vk::ImageType::e2D)
            .setTiling(vk::ImageTiling::eOptimal)
            .setSamples(vk::SampleCountFlagBits::e1)
            .setInitialLayout(vk::ImageLayout::eUndefined);

    auto ColorAllocateInfo = vma::AllocationCreateInfo{}
            .setFlags(vma::AllocationCreateFlagBits::eStrategyBestFit)
            .setUsage(vma::MemoryUsage::eAutoPreferDevice);

    auto DepthCreateInfo = vk::ImageCreateInfo{}
            .setUsage(vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled)
            .setExtent(vk::Extent3D{Width, Height, 1})
            .setArrayLayers(1)
            .setMipLevels(1)
            .setFormat(GBuffer.DepthFormat)
            .setImageType(vk::ImageType::e2D)
            .setTiling(vk::ImageTiling::eOptimal)
            .setSamples(vk::SampleCountFlagBits::e1)
            .setInitialLayout(vk::ImageLayout::eUndefined);

    auto DepthAllocateInfo = vma::AllocationCreateInfo{}
            .setFlags(vma::AllocationCreateFlagBits::eStrategyBestFit)
            .setUsage(vma::MemoryUsage::eAutoPreferDevice);


    vkResultCheck = Allocator.createImage(&PositionCreateInfo, &PositionAllocateInfo, &GBuffer.Position.Image, &GBuffer.Position.Allocation, &GBuffer.Position.Info);
    vkResultCheck = Allocator.createImage(&NormalCreateInfo, &NormalAllocateInfo, &GBuffer.Normal.Image, &GBuffer.Normal.Allocation, &GBuffer.Normal.Info);
    vkResultCheck = Allocator.createImage(&ColorCreateInfo, &ColorAllocateInfo, &GBuffer.Color.Image, &GBuffer.Color.Allocation, &GBuffer.Color.Info);
    vkResultCheck = Allocator.createImage(&DepthCreateInfo, &DepthAllocateInfo, &GBuffer.Depth.Image, &GBuffer.Depth.Allocation, &GBuffer.Depth.Info);

    NameObject(GBuffer.Position.Image, "GBuffer Position image");
    NameObject(GBuffer.Normal.Image, "GBuffer Normal image");
    NameObject(GBuffer.Color.Image, "GBuffer Color image");
    NameObject(GBuffer.Depth.Image, "GBuffer Depth image");

    auto PositionViewCreateInfo = vk::ImageViewCreateInfo{}
            .setImage(GBuffer.Position.Image)
            .setFormat(GBuffer.PositionFormat)
            .setViewType(vk::ImageViewType::e2D)
            .setComponents(vk::ComponentMapping{})
            .setSubresourceRange(vkutil::flat_subresource_range(vk::ImageAspectFlagBits::eColor));

    auto NormalViewCreateInfo = vk::ImageViewCreateInfo{}
            .setImage(GBuffer.Normal.Image)
            .setFormat(GBuffer.NormalFormat)
            .setViewType(vk::ImageViewType::e2D)
            .setComponents(vk::ComponentMapping{})
            .setSubresourceRange(vkutil::flat_subresource_range(vk::ImageAspectFlagBits::eColor));

    auto ColorViewCreateInfo = vk::ImageViewCreateInfo{}
            .setImage(GBuffer.Color.Image)
            .setFormat(GBuffer.ColorFormat)
            .setViewType(vk::ImageViewType::e2D)
            .setComponents(vk::ComponentMapping{})
            .setSubresourceRange(vkutil::flat_subresource_range(vk::ImageAspectFlagBits::eColor));

    auto DepthViewCreateInfo = vk::ImageViewCreateInfo{}
            .setImage(GBuffer.Depth.Image)
            .setFormat(GBuffer.DepthFormat)
            .setViewType(vk::ImageViewType::e2D)
            .setComponents(vk::ComponentMapping{})
            .setSubresourceRange(vkutil::flat_subresource_range(vk::ImageAspectFlagBits::eDepth));

    vkResultCheck = Device.createImageView(&PositionViewCreateInfo, nullptr, &GBuffer.Position.ImageView);
    vkResultCheck = Device.createImageView(&NormalViewCreateInfo, nullptr, &GBuffer.Normal.ImageView);
    vkResultCheck = Device.createImageView(&ColorViewCreateInfo, nullptr, &GBuffer.Color.ImageView);
    vkResultCheck = Device.createImageView(&DepthViewCreateInfo, nullptr, &GBuffer.Depth.ImageView);

    NameObject(GBuffer.Position.ImageView, "GBuffer Position image view");
    NameObject(GBuffer.Normal.ImageView, "GBuffer Normal image view");
    NameObject(GBuffer.Color.ImageView, "GBuffer Color image view");
    NameObject(GBuffer.Depth.ImageView, "GBuffer Depth image view");
}

void VStarSightRenderer::Draw(uint32_t MeshCount)
{
    uint32_t SwapChainImage = AcquireSwapChainImage();
    if(SwapChainImage == UINT32_MAX)
    {
        RecreateSwapChain();
        return;
    }

    ActiveFrame->CommandBuffer.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    ActiveFrame->CommandBuffer.fillBuffer(DrawIndirectCommandsBuffer.Buffer, 0, sizeof(uint32_t), 0u);

    auto ZeroDrawCountBarrier = vk::BufferMemoryBarrier2{}
            .setBuffer(DrawIndirectCommandsBuffer.Buffer)
            .setSize(sizeof(uint32_t))
            .setOffset(0)
            .setSrcStageMask(vk::PipelineStageFlagBits2::eTransfer)
            .setSrcAccessMask(vk::AccessFlagBits2::eTransferWrite)
            .setDstStageMask(vk::PipelineStageFlagBits2::eComputeShader)
            .setDstAccessMask(vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite);

    auto ZeroDrawCount_Dependency = vk::DependencyInfo{}.setBufferMemoryBarriers(ZeroDrawCountBarrier);
    ActiveFrame->CommandBuffer.pipelineBarrier2(ZeroDrawCount_Dependency);

    VShaderBuildDrawCommandsPC PushConstants{};
    PushConstants.pCamera = CameraBuffer.BufferAddress + (ActiveFrameIndex() * sizeof(VShaderCameraData));
    PushConstants.pDrawIndirectCount = DrawIndirectCommandsBuffer.BufferAddress;
    PushConstants.pMeshBounds = ActiveFrame->MeshBounds.BufferAddress;
    PushConstants.pMeshes = ActiveFrame->MeshInfos.BufferAddress;
    PushConstants.pTransforms = ActiveFrame->MeshTransforms.BufferAddress;
    PushConstants.meshCount = MeshCount;

    ActiveFrame->CommandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, BuildDrawCommandsPipeline);
    ActiveFrame->CommandBuffer.pushConstants(BuildDrawCommandsLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(PushConstants), &PushConstants);
    ActiveFrame->CommandBuffer.dispatch(vkutil::GroupCount(MeshCount, 64u), 1u, 1u);

    auto DrawIndirectCommandsBarrier = vk::BufferMemoryBarrier2{}
            .setBuffer(DrawIndirectCommandsBuffer.Buffer)
            .setOffset(0)
            .setSize(VK_WHOLE_SIZE)
            .setDstStageMask(vk::PipelineStageFlagBits2::eDrawIndirect)
            .setDstAccessMask(vk::AccessFlagBits2::eIndirectCommandRead)
            .setSrcStageMask(vk::PipelineStageFlagBits2::eComputeShader)
            .setSrcAccessMask(vk::AccessFlagBits2::eShaderStorageWrite);

    auto DrawIndirectCommandsBarrier_Dependency = vk::DependencyInfo{}.setBufferMemoryBarriers(DrawIndirectCommandsBarrier);
    ActiveFrame->CommandBuffer.pipelineBarrier2(DrawIndirectCommandsBarrier_Dependency);

    BeginSwapChainRender(SwapChainImage);
    DrawShader(MeshCount);
    EndSwapChainRender(SwapChainImage);

    ActiveFrame->CommandBuffer.end();

    SubmitCommands();

    if(!PresentImage(SwapChainImage))
    {
        RecreateSwapChain();
    }
    else
    {
        AdvanceActiveFrame();
    }
}

uint32_t VStarSightRenderer::AcquireSwapChainImage()
{
    vkResultCheck = Device.waitForFences(ActiveFrame->InFlight, true, vkutil::default_timeout);

    auto AcquireInfo = vk::AcquireNextImageInfoKHR{}
            .setSwapchain(SwapChain)
            .setSemaphore(ActiveFrame->ImageAvailable)
            .setFence(nullptr)
            .setTimeout(vkutil::default_timeout)
            .setDeviceMask(1);

    uint32_t ImageIndex = UINT32_MAX;
    vk::Result Result = Device.acquireNextImage2KHR(&AcquireInfo, &ImageIndex);

    for(auto& OnBegin : ActiveFrame->OnFrameBegin)
    {
        OnBegin();
    }

    ActiveFrame->OnFrameBegin.clear();

    std::function<void()> OnNextFrame;
    while(DeferredDestructionQueue.try_dequeue(OnNextFrame))
    {
        ActiveFrame->OnFrameBegin.emplace_back(std::move(OnNextFrame));
    }

    if(Result == vk::Result::eErrorOutOfDateKHR)
    {
        return UINT32_MAX;
    }
    else if(Result != vk::Result::eSuccess && Result != vk::Result::eSuboptimalKHR)
    {
        VERIFY(false, vk::to_string(Result));
    }

    Device.resetFences(ActiveFrame->InFlight);

    return ImageIndex;
}

void VStarSightRenderer::BeginSwapChainRender(uint32_t SwapChainImage)
{
    vk::ClearValue ColorClear{{0.f, 0.f, 0.f, 1.f}};
    vk::ClearValue DepthCleat{vk::ClearDepthStencilValue{0.0, 0}};

    auto ColorAttachment = vk::RenderingAttachmentInfo{}
            .setClearValue(ColorClear)
            .setImageView(ImageViews[SwapChainImage])
            .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eStore);

    auto DepthAttachment = vk::RenderingAttachmentInfo{}
            .setClearValue(DepthCleat)
            .setImageView(GBuffer.Depth.ImageView)
            .setImageLayout(vk::ImageLayout::eDepthAttachmentOptimal)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eStore);

    vk::Rect2D RenderArea{
            vk::Offset2D{0, 0},
            vk::Extent2D{ImageExtent.width, ImageExtent.height}
    };

    auto RenderingInfo = vk::RenderingInfo{}
            .setRenderArea(RenderArea)
            .setColorAttachments(ColorAttachment)
            .setPDepthAttachment(&DepthAttachment)
            .setLayerCount(1);

    auto SwapChainImage2ColorAttachment = vk::ImageMemoryBarrier2{}
            .setImage(Images[SwapChainImage])
            .setOldLayout(vk::ImageLayout::eUndefined)
            .setNewLayout(vk::ImageLayout::eColorAttachmentOptimal)
            .setSrcStageMask(PipelineStage::eNone)
            .setSrcAccessMask(AccessFlag::eNone)
            .setDstStageMask(PipelineStage::eColorAttachmentOutput)
            .setDstAccessMask(AccessFlag::eColorAttachmentRead | AccessFlag::eColorAttachmentWrite)
            .setSubresourceRange(vkutil::flat_subresource_range(vk::ImageAspectFlagBits::eColor));

    auto Depth2DepthAttachment = vk::ImageMemoryBarrier2{}
            .setImage(GBuffer.Depth.Image)
            .setOldLayout(vk::ImageLayout::eUndefined)
            .setNewLayout(vk::ImageLayout::eDepthAttachmentOptimal)
            .setSrcStageMask(PipelineStage::eEarlyFragmentTests | PipelineStage::eLateFragmentTests)
            .setSrcAccessMask(AccessFlag::eDepthStencilAttachmentRead | AccessFlag::eDepthStencilAttachmentWrite)
            .setDstStageMask(PipelineStage::eEarlyFragmentTests | PipelineStage::eLateFragmentTests)
            .setDstAccessMask(AccessFlag::eDepthStencilAttachmentRead | AccessFlag::eDepthStencilAttachmentWrite)
            .setSubresourceRange(vkutil::flat_subresource_range(vk::ImageAspectFlagBits::eDepth));

    std::array ImageMemoryBarriers{
            SwapChainImage2ColorAttachment,
            Depth2DepthAttachment
    };

    auto Dependencies = vk::DependencyInfo{}
            .setImageMemoryBarriers(ImageMemoryBarriers);

    vk::Viewport Viewport{
            0,
            0,
            static_cast<float>(ImageExtent.width),
            static_cast<float>(ImageExtent.height),
            0.0,
            1.0
    };

    ActiveFrame->CommandBuffer.pipelineBarrier2(Dependencies);
    ActiveFrame->CommandBuffer.beginRendering(RenderingInfo);
    ActiveFrame->CommandBuffer.setViewport(0, Viewport);
    ActiveFrame->CommandBuffer.setScissor(0, RenderArea);
}

void VStarSightRenderer::DrawShader(uint32_t MeshCount)
{
    VShaderForwardDrawPC PushConstants{};
    PushConstants.pCamera = CameraBuffer.BufferAddress + (ActiveFrameIndex() * sizeof(VShaderCameraData));
    PushConstants.pMesh = ActiveFrame->MeshInfos.BufferAddress;
    PushConstants.pTransform = ActiveFrame->MeshTransforms.BufferAddress;
    PushConstants.pVertexBuffer = GlobalVertexBuffer.BufferAddress;

    ActiveFrame->CommandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, ForwardPipeline);
    ActiveFrame->CommandBuffer.pushConstants(ForwardPipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(PushConstants), &PushConstants);
    ActiveFrame->CommandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, ForwardPipelineLayout, 0, 1, &ShaderResourceSet, 0, nullptr);
    ActiveFrame->CommandBuffer.bindIndexBuffer(GlobalIndexBuffer.Buffer, 0, vk::IndexType::eUint32);
    ActiveFrame->CommandBuffer.drawIndexedIndirectCount(DrawIndirectCommandsBuffer.Buffer, sizeof(VShaderDrawIndirectCount), DrawIndirectCommandsBuffer.Buffer, 0, MeshCount, sizeof(vk::DrawIndexedIndirectCommand));
}

void VStarSightRenderer::EndSwapChainRender(uint32_t SwapChainImage)
{
    auto SwapChainImage2PresentSrc = vk::ImageMemoryBarrier2{}
            .setImage(Images[SwapChainImage])
            .setOldLayout(vk::ImageLayout::eColorAttachmentOptimal)
            .setNewLayout(vk::ImageLayout::ePresentSrcKHR)
            .setSrcStageMask(PipelineStage::eColorAttachmentOutput)
            .setSrcAccessMask(AccessFlag::eColorAttachmentWrite)
            .setDstStageMask(PipelineStage::eColorAttachmentOutput)
            .setDstAccessMask(AccessFlag::eNone)
            .setSubresourceRange(vkutil::flat_subresource_range(vk::ImageAspectFlagBits::eColor));

    auto Dependency = vk::DependencyInfo{}
            .setImageMemoryBarriers(SwapChainImage2PresentSrc);

    ActiveFrame->CommandBuffer.endRendering();
    ActiveFrame->CommandBuffer.pipelineBarrier2(Dependency);
}

void VStarSightRenderer::SubmitCommands()
{
    auto CommandInfo = vk::CommandBufferSubmitInfo{}
            .setCommandBuffer(ActiveFrame->CommandBuffer);

    auto WaitInfo = vk::SemaphoreSubmitInfo{}
            .setSemaphore(ActiveFrame->ImageAvailable)
            .setStageMask(PipelineStage::eColorAttachmentOutput);

    auto SignalInfo = vk::SemaphoreSubmitInfo{}
            .setSemaphore(ActiveFrame->DrawFinished)
            .setStageMask(PipelineStage::eColorAttachmentOutput);

    auto SubmitInfo = vk::SubmitInfo2{}
            .setCommandBufferInfos(CommandInfo)
            .setWaitSemaphoreInfos(WaitInfo)
            .setSignalSemaphoreInfos(SignalInfo);

    QueueHandles.Graphics.submit2(SubmitInfo, ActiveFrame->InFlight);
}

bool VStarSightRenderer::PresentImage(uint32_t SwapChainImage)
{
    vk::Result Result{};
    auto PresentInfo = vk::PresentInfoKHR{}
            .setImageIndices(SwapChainImage)
            .setWaitSemaphores(ActiveFrame->DrawFinished)
            .setSwapchains(SwapChain)
            .setResults(Result);

    (void)QueueHandles.Present.presentKHR(PresentInfo);

    if(Result == vk::Result::eErrorOutOfDateKHR || Result == vk::Result::eSuboptimalKHR)
    {
        return false;
    }
    else if(Result != vk::Result::eSuccess)
    {
        VERIFY(false, vk::to_string(Result));
    }

    return true;
}

void VStarSightRenderer::AdvanceActiveFrame()
{
    if(ActiveFrame == Frames.end() - 1)
    {
        ActiveFrame = Frames.begin();
    }
    else
    {
        ++ActiveFrame;
    }
}

std::string_view VStarSightRenderer::GetWindowName() const
{
    return GlfwGetWindowUserData(Window)->WindowName;
}

std::pair<uint32_t, uint32_t> VStarSightRenderer::GetWindowExtent()
{
    int width; int height;
    glfwGetFramebufferSize(Window, &width, &height);
    return {width, height};
}

void VStarSightRenderer::WaitForFrames()
{
    std::array<vk::Fence, FRAMES_IN_FLIGHT> FrameFences{};
    for(uint64_t index = 0; index < FRAMES_IN_FLIGHT; ++index)
    {
        FrameFences[index] = Frames[index].InFlight;
    }

    constexpr uint64_t TimeOut = std::nano::den * 10;
    vkResultCheck = Device.waitForFences(FrameFences, true, TimeOut);
}

void VStarSightRenderer::DestroyGBuffer()
{
    Allocator.destroyImage(GBuffer.Position.Image, GBuffer.Position.Allocation);
    Device.destroyImageView(GBuffer.Position.ImageView);
    Allocator.destroyImage(GBuffer.Normal.Image, GBuffer.Normal.Allocation);
    Device.destroyImageView(GBuffer.Normal.ImageView);
    Allocator.destroyImage(GBuffer.Color.Image, GBuffer.Color.Allocation);
    Device.destroyImageView(GBuffer.Color.ImageView);
    Allocator.destroyImage(GBuffer.Depth.Image, GBuffer.Depth.Allocation);
    Device.destroyImageView(GBuffer.Depth.ImageView);
}
/*
void VRenderTarget::DrawDeferred(std::span<ecs::RenderSystem::RenderInfo> RenderInfos)
{
    uint32_t SwapChainImage = AcquireSwapChainImage();
    if(SwapChainImage == UINT32_MAX)
    {
        RecreateSwapChain();
        return;
    }

    ActiveFrame->CommandBuffer.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    BeginGeometryPass();
    RenderGBuffer(RenderInfos);
    EndGeometryPass(SwapChainImage);

    ActiveFrame->CommandBuffer.end();

    SubmitGBufferCommands();

    if(!PresentImage(SwapChainImage))
    {
        RecreateSwapChain();
    }
    else
    {
        AdvanceActiveFrame();
    }
}
*/
void VStarSightRenderer::BeginGeometryPass()
{
    auto Undefined2ColorAttachmentOptimal = [](vk::Image Image)
    {
        return vk::ImageMemoryBarrier2{}
                .setImage(Image)
                .setOldLayout(vk::ImageLayout::eUndefined)
                .setNewLayout(vk::ImageLayout::eColorAttachmentOptimal)
                .setSrcStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
                .setSrcAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentRead)
                .setDstStageMask(vk::PipelineStageFlagBits2::eClear | vk::PipelineStageFlagBits2::eColorAttachmentOutput)
                .setDstAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)
                .setSubresourceRange(vk::ImageSubresourceRange{
                        vk::ImageAspectFlagBits::eColor,
                        0,
                        1,
                        0,
                        1
                });
    };

    std::array<vk::ImageMemoryBarrier2, 4> GBufferBarriers{};
    GBufferBarriers[0] = Undefined2ColorAttachmentOptimal(GBuffer.Position.Image);
    GBufferBarriers[1] = Undefined2ColorAttachmentOptimal(GBuffer.Normal.Image);
    GBufferBarriers[2] = Undefined2ColorAttachmentOptimal(GBuffer.Color.Image);
    GBufferBarriers[3]
            .setImage(GBuffer.Depth.Image)
            .setOldLayout(vk::ImageLayout::eUndefined)
            .setNewLayout(vk::ImageLayout::eDepthAttachmentOptimal)
            .setSrcStageMask(vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests)
            .setSrcAccessMask(vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite)
            .setDstStageMask(vk::PipelineStageFlagBits2::eClear | vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests)
            .setDstAccessMask(vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite)
            .setSubresourceRange(vk::ImageSubresourceRange{
                    vk::ImageAspectFlagBits::eDepth,
                    0,
                    1,
                    0,
                    1
            });

    vk::ClearValue ColorClearValue{vk::ClearColorValue{0, 0, 0, 0}};
    vk::ClearValue DepthClearVale{vk::ClearDepthStencilValue{0.0, 0}};

    std::array<vk::RenderingAttachmentInfo, 3> ColorAttachments{};
    ColorAttachments[0]
            .setImageView(GBuffer.Position.ImageView)
            .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eStore)
            .setClearValue(ColorClearValue);

    ColorAttachments[1]
            .setImageView(GBuffer.Normal.ImageView)
            .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eStore)
            .setClearValue(ColorClearValue);

    ColorAttachments[2]
            .setImageView(GBuffer.Color.ImageView)
            .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eStore)
            .setClearValue(ColorClearValue);

    auto DepthAttachment = vk::RenderingAttachmentInfo{}
            .setImageView(GBuffer.Depth.ImageView)
            .setImageLayout(vk::ImageLayout::eDepthAttachmentOptimal)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eStore)
            .setClearValue(DepthClearVale);

    vk::Rect2D RenderArea{
            vk::Offset2D{0, 0},
            vk::Extent2D{ImageExtent.width, ImageExtent.height}
    };

    auto RenderingInfo = vk::RenderingInfo{}
            .setColorAttachments(ColorAttachments)
            .setPDepthAttachment(&DepthAttachment)
            .setRenderArea(RenderArea)
            .setLayerCount(1)
            .setViewMask(0);

    vk::Viewport Viewport{
            0,
            0,
            static_cast<float>(ImageExtent.width),
            static_cast<float>(ImageExtent.height),
            0.0,
            1.0
    };

    ActiveFrame->CommandBuffer.pipelineBarrier2(vk::DependencyInfo{}.setImageMemoryBarriers(GBufferBarriers));
    ActiveFrame->CommandBuffer.beginRendering(RenderingInfo);
    ActiveFrame->CommandBuffer.setViewport(0, Viewport);
    ActiveFrame->CommandBuffer.setScissor(0, RenderArea);
}
/*
void VRenderTarget::RenderGBuffer(std::span<ecs::RenderSystem::RenderInfo> RenderInfos)
{
    std::span<ecs::TransformComponent> Transforms = ActiveWorld->GetComponentsOfType<ecs::TransformComponent>();

    VModel* LastModel = nullptr;
    VTexture* LastTexture = nullptr;

    ActiveFrame->CommandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, GeometryPipeline);

    for(auto& RenderInfo : RenderInfos)
    {
        ShaderPushConstant PushConstant{};
        PushConstant.ViewProjection = Projection * View;
        PushConstant.Time = float(ShaderProgress / AnimationTime);
        PushConstant.Screen.x = ImageExtent.width;
        PushConstant.Screen.y = ImageExtent.height;
        PushConstant.Location = Transforms[RenderInfo.TransformIndex].Location;
        PushConstant.Rotation = Transforms[RenderInfo.TransformIndex].Rotation;
        PushConstant.Scale = Transforms[RenderInfo.TransformIndex].Scale;

        std::array<vk::DescriptorSet, 2> DescriptorSets{RenderInfo.Model->VertexSet, RenderInfo.Texture->TextureSet};

        if(LastModel != RenderInfo.Model || LastTexture != RenderInfo.Texture)
        {
            ActiveFrame->CommandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, GeometryPipelineLayout, 0, 2, DescriptorSets.data(), 0,nullptr);

            LastModel = RenderInfo.Model;
            LastTexture = RenderInfo.Texture;
        }

        ActiveFrame->CommandBuffer.pushConstants(GeometryPipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(ShaderPushConstant), &PushConstant);
        ActiveFrame->CommandBuffer.draw(RenderInfo.Model->VertexCount, 1, 0, 0);
    }
}
*/
void VStarSightRenderer::EndGeometryPass(uint32_t SwapChainImag)
{
    auto SwapChainImage2PresentSrc = vk::ImageMemoryBarrier2{}
            .setImage(Images[SwapChainImag])
            .setOldLayout(vk::ImageLayout::eUndefined)
            .setNewLayout(vk::ImageLayout::ePresentSrcKHR)
            .setSrcStageMask(PipelineStage::eColorAttachmentOutput)
            .setSrcAccessMask(AccessFlag::eColorAttachmentWrite)
            .setDstStageMask(PipelineStage::eColorAttachmentOutput)
            .setDstAccessMask(AccessFlag::eNone)
            .setSubresourceRange(vkutil::flat_subresource_range(vk::ImageAspectFlagBits::eColor));

    auto Dependency = vk::DependencyInfo{}
            .setImageMemoryBarriers(SwapChainImage2PresentSrc);

    ActiveFrame->CommandBuffer.endRendering();
    ActiveFrame->CommandBuffer.pipelineBarrier2(Dependency);
}

void VStarSightRenderer::SubmitGBufferCommands()
{
    auto CommandInfo = vk::CommandBufferSubmitInfo{}
            .setCommandBuffer(ActiveFrame->CommandBuffer);

    auto WaitInfo = vk::SemaphoreSubmitInfo{}
            .setSemaphore(ActiveFrame->ImageAvailable)
            .setStageMask(PipelineStage::eColorAttachmentOutput);

    auto SignalInfo = vk::SemaphoreSubmitInfo{}
            .setSemaphore(ActiveFrame->DrawFinished)
            .setStageMask(PipelineStage::eColorAttachmentOutput);

    auto SubmitInfo = vk::SubmitInfo2{}
            .setCommandBufferInfos(CommandInfo)
            .setWaitSemaphoreInfos(WaitInfo)
            .setSignalSemaphoreInfos(SignalInfo);

    QueueHandles.Graphics.submit2(SubmitInfo, ActiveFrame->InFlight);
}

uint64_t VStarSightRenderer::ActiveFrameIndex() const
{
    return std::distance(Frames.data(), static_cast<const VFrame*>(ActiveFrame));
}

void VStarSightRenderer::CreateCameraBuffer()
{
    LOG_INFO("creating camera buffer");

    CameraBuffer = AllocateBuffer(
            sizeof(VShaderCameraData) * FRAMES_IN_FLIGHT,
            vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
            vma::AllocationCreateFlagBits::eMapped | vma::AllocationCreateFlagBits::eHostAccessSequentialWrite | vma::AllocationCreateFlagBits::eStrategyBestFit,
            vma::MemoryUsage::eAutoPreferDevice,
            "camera buffer");

    DestructionQueue.emplace_back([this](){
        Allocator.destroyBuffer(CameraBuffer.Buffer, CameraBuffer.Allocation);
    });
}

void VStarSightRenderer::CreateIndirectCommandsBuffer()
{
    LOG_INFO("creating indirect draw buffer");

    DrawIndirectCommandsBuffer = AllocateBuffer(
            sizeof(VShaderDrawIndirectCount) + (sizeof(vk::DrawIndexedIndirectCommand) * DEVICE_MESH_ALLOCATION_STEP),
            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddress,
            vma::AllocationCreateFlagBits::eStrategyBestFit,
            vma::MemoryUsage::eAutoPreferDevice,
            "indirect draw buffer");

    DestructionQueue.emplace_back([this](){
        Allocator.destroyBuffer(DrawIndirectCommandsBuffer.Buffer, DrawIndirectCommandsBuffer.Allocation);
    });
}

void VStarSightRenderer::DrawDeferred(uint32_t MeshCount)
{
    uint32_t SwapChainImage = AcquireSwapChainImage();
    if(SwapChainImage == UINT32_MAX)
    {
        RecreateSwapChain();
        return;
    }

    ActiveFrame->CommandBuffer.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    ActiveFrame->CommandBuffer.fillBuffer(DrawIndirectCommandsBuffer.Buffer, 0, sizeof(uint32_t), 0u);

    auto ZeroDrawCountBarrier = vk::BufferMemoryBarrier2{}
            .setBuffer(DrawIndirectCommandsBuffer.Buffer)
            .setSize(sizeof(uint32_t))
            .setOffset(0)
            .setSrcStageMask(vk::PipelineStageFlagBits2::eTransfer)
            .setSrcAccessMask(vk::AccessFlagBits2::eTransferWrite)
            .setDstStageMask(vk::PipelineStageFlagBits2::eComputeShader)
            .setDstAccessMask(vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite);

    auto ZeroDrawCount_Dependency = vk::DependencyInfo{}.setBufferMemoryBarriers(ZeroDrawCountBarrier);
    ActiveFrame->CommandBuffer.pipelineBarrier2(ZeroDrawCount_Dependency);

    VShaderBuildDrawCommandsPC DrawCommandsPushConstants{};
    DrawCommandsPushConstants.pCamera = CameraBuffer.BufferAddress + (ActiveFrameIndex() * sizeof(VShaderCameraData));
    DrawCommandsPushConstants.pDrawIndirectCount = DrawIndirectCommandsBuffer.BufferAddress;
    DrawCommandsPushConstants.pMeshBounds = ActiveFrame->MeshBounds.BufferAddress;
    DrawCommandsPushConstants.pMeshes = ActiveFrame->MeshInfos.BufferAddress;
    DrawCommandsPushConstants.pTransforms = ActiveFrame->MeshTransforms.BufferAddress;
    DrawCommandsPushConstants.meshCount = MeshCount;

    ActiveFrame->CommandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, BuildDrawCommandsPipeline);
    ActiveFrame->CommandBuffer.pushConstants(BuildDrawCommandsLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(DrawCommandsPushConstants), &DrawCommandsPushConstants);
    ActiveFrame->CommandBuffer.dispatch(vkutil::GroupCount(MeshCount, 64u), 1u, 1u);

    auto DrawIndirectCommandsBarrier = vk::BufferMemoryBarrier2{}
            .setBuffer(DrawIndirectCommandsBuffer.Buffer)
            .setOffset(0)
            .setSize(VK_WHOLE_SIZE)
            .setDstStageMask(vk::PipelineStageFlagBits2::eDrawIndirect)
            .setDstAccessMask(vk::AccessFlagBits2::eIndirectCommandRead)
            .setSrcStageMask(vk::PipelineStageFlagBits2::eComputeShader)
            .setSrcAccessMask(vk::AccessFlagBits2::eShaderStorageWrite);

    auto DrawIndirectCommandsBarrier_Dependency = vk::DependencyInfo{}.setBufferMemoryBarriers(DrawIndirectCommandsBarrier);
    ActiveFrame->CommandBuffer.pipelineBarrier2(DrawIndirectCommandsBarrier_Dependency);

    auto Undefined2ColorAttachmentOptimal = [](vk::Image Image)
    {
        return vk::ImageMemoryBarrier2{}
                .setImage(Image)
                .setOldLayout(vk::ImageLayout::eUndefined)
                .setNewLayout(vk::ImageLayout::eColorAttachmentOptimal)
                .setSrcStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
                .setSrcAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentRead)
                .setDstStageMask(vk::PipelineStageFlagBits2::eClear | vk::PipelineStageFlagBits2::eColorAttachmentOutput)
                .setDstAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)
                .setSubresourceRange(vk::ImageSubresourceRange{
                        vk::ImageAspectFlagBits::eColor,
                        0,
                        1,
                        0,
                        1
                });
    };

    std::array<vk::ImageMemoryBarrier2, 4> GBufferBarriers{};
    GBufferBarriers[0] = Undefined2ColorAttachmentOptimal(GBuffer.Position.Image);
    GBufferBarriers[1] = Undefined2ColorAttachmentOptimal(GBuffer.Normal.Image);
    GBufferBarriers[2] = Undefined2ColorAttachmentOptimal(GBuffer.Color.Image);
    GBufferBarriers[3]
            .setImage(GBuffer.Depth.Image)
            .setOldLayout(vk::ImageLayout::eUndefined)
            .setNewLayout(vk::ImageLayout::eDepthAttachmentOptimal)
            .setSrcStageMask(vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests)
            .setSrcAccessMask(vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite)
            .setDstStageMask(vk::PipelineStageFlagBits2::eClear | vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests)
            .setDstAccessMask(vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite)
            .setSubresourceRange(vk::ImageSubresourceRange{
                    vk::ImageAspectFlagBits::eDepth,
                    0,
                    1,
                    0,
                    1
            });

    vk::ClearValue ColorClearValue{vk::ClearColorValue{0, 0, 0, 0}};
    vk::ClearValue DepthClearVale{vk::ClearDepthStencilValue{0.0, 0}};

    std::array<vk::RenderingAttachmentInfo, 3> ColorAttachments{};
    ColorAttachments[0]
            .setImageView(GBuffer.Position.ImageView)
            .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eStore)
            .setClearValue(ColorClearValue);

    ColorAttachments[1]
            .setImageView(GBuffer.Normal.ImageView)
            .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eStore)
            .setClearValue(ColorClearValue);

    ColorAttachments[2]
            .setImageView(GBuffer.Color.ImageView)
            .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eStore)
            .setClearValue(ColorClearValue);

    auto DepthAttachment = vk::RenderingAttachmentInfo{}
            .setImageView(GBuffer.Depth.ImageView)
            .setImageLayout(vk::ImageLayout::eDepthAttachmentOptimal)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eStore)
            .setClearValue(DepthClearVale);

    vk::Rect2D RenderArea{
            vk::Offset2D{0, 0},
            vk::Extent2D{ImageExtent.width, ImageExtent.height}
    };

    auto RenderingInfo = vk::RenderingInfo{}
            .setColorAttachments(ColorAttachments)
            .setPDepthAttachment(&DepthAttachment)
            .setRenderArea(RenderArea)
            .setLayerCount(1)
            .setViewMask(0);

    vk::Viewport Viewport{
            0,
            0,
            static_cast<float>(ImageExtent.width),
            static_cast<float>(ImageExtent.height),
            0.0,
            1.0
    };

    ActiveFrame->CommandBuffer.pipelineBarrier2(vk::DependencyInfo{}.setImageMemoryBarriers(GBufferBarriers));
    ActiveFrame->CommandBuffer.beginRendering(RenderingInfo);
    ActiveFrame->CommandBuffer.setViewport(0, Viewport);
    ActiveFrame->CommandBuffer.setScissor(0, RenderArea);

    VShaderForwardDrawPC GeometryPushConstants{};
    GeometryPushConstants.pCamera = CameraBuffer.BufferAddress + (ActiveFrameIndex() * sizeof(VShaderCameraData));
    GeometryPushConstants.pMesh = ActiveFrame->MeshInfos.BufferAddress;
    GeometryPushConstants.pTransform = ActiveFrame->MeshTransforms.BufferAddress;
    GeometryPushConstants.pVertexBuffer = GlobalVertexBuffer.BufferAddress;

    ActiveFrame->CommandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, GeometryPipeline);
    ActiveFrame->CommandBuffer.pushConstants(GeometryPipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(GeometryPushConstants), &GeometryPushConstants);
    ActiveFrame->CommandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, GeometryPipelineLayout, 0, 1, &ShaderResourceSet, 0, nullptr);
    ActiveFrame->CommandBuffer.bindIndexBuffer(GlobalIndexBuffer.Buffer, 0, vk::IndexType::eUint32);
    ActiveFrame->CommandBuffer.drawIndexedIndirectCount(DrawIndirectCommandsBuffer.Buffer, sizeof(VShaderDrawIndirectCount), DrawIndirectCommandsBuffer.Buffer, 0, MeshCount, sizeof(vk::DrawIndexedIndirectCommand));

    ActiveFrame->CommandBuffer.endRendering();

    VDescriptorLayoutCache::layout_info_t GlobalLightDescriptorLayoutInfo{};

    GlobalLightDescriptorLayoutInfo.flags.emplace_back();
    GlobalLightDescriptorLayoutInfo.bindings.emplace_back()
            .setBinding(0)
            .setDescriptorCount(1)
            .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
            .setStageFlags(vk::ShaderStageFlagBits::eCompute);

    GlobalLightDescriptorLayoutInfo.flags.emplace_back();
    GlobalLightDescriptorLayoutInfo.bindings.emplace_back()
            .setBinding(1)
            .setDescriptorCount(1)
            .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
            .setStageFlags(vk::ShaderStageFlagBits::eCompute);

    GlobalLightDescriptorLayoutInfo.flags.emplace_back();
    GlobalLightDescriptorLayoutInfo.bindings.emplace_back()
            .setBinding(2)
            .setDescriptorCount(1)
            .setDescriptorType(vk::DescriptorType::eStorageImage)
            .setStageFlags(vk::ShaderStageFlagBits::eCompute);

    vk::DescriptorSetLayout GlobalLightDescriptorLayout = DescriptorLayoutCache->create_layout(GlobalLightDescriptorLayoutInfo);

    auto GlobalLightDescriptorAllocateInfo = vk::DescriptorSetAllocateInfo{}
    .setDescriptorPool(TransientDescriptorPool)
    .setDescriptorSetCount(1)
    .setSetLayouts(GlobalLightDescriptorLayout);

    vk::DescriptorSet GlobalLightDescriptorSet = nullptr;
    vkResultCheck = Device.allocateDescriptorSets(&GlobalLightDescriptorAllocateInfo, &GlobalLightDescriptorSet);

    auto GBufferSamplerInfo = vk::SamplerCreateInfo{}
            .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
            .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
            .setAddressModeW(vk::SamplerAddressMode::eClampToEdge)
            .setAnisotropyEnable(false)
            .setMaxAnisotropy(0)
            .setUnnormalizedCoordinates(true)
            .setCompareEnable(false)
            .setMipmapMode(vk::SamplerMipmapMode::eNearest)
            .setMinFilter(vk::Filter::eLinear)
            .setMagFilter(vk::Filter::eLinear)
            .setMinLod(0.f)
            .setMaxLod(0.f);

    vk::Sampler GBufferSampler = Device.createSampler(GBufferSamplerInfo);

    std::array<vk::DescriptorImageInfo, 3> DescriptorImageInfos{};
    DescriptorImageInfos[0]
            .setImageView(GBuffer.Normal.ImageView)
            .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
            .setSampler(GBufferSampler);

    DescriptorImageInfos[1]
            .setImageView(GBuffer.Color.ImageView)
            .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
            .setSampler(GBufferSampler);

    DescriptorImageInfos[2]
            .setImageView(ImageViews[SwapChainImage])
            .setImageLayout(vk::ImageLayout::eGeneral)
            .setSampler(nullptr);

    std::array<vk::WriteDescriptorSet, 3> DescriptorSetWrites{};
    DescriptorSetWrites[0]
            .setDstSet(GlobalLightDescriptorSet)
            .setDstArrayElement(0)
            .setDstBinding(0)
            .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
            .setImageInfo(DescriptorImageInfos[0]);

    DescriptorSetWrites[1]
            .setDstSet(GlobalLightDescriptorSet)
            .setDstArrayElement(0)
            .setDstBinding(1)
            .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
            .setImageInfo(DescriptorImageInfos[1]);

    DescriptorSetWrites[2]
            .setDstSet(GlobalLightDescriptorSet)
            .setDstArrayElement(0)
            .setDstBinding(2)
            .setDescriptorType(vk::DescriptorType::eStorageImage)
            .setImageInfo(DescriptorImageInfos[2]);

    Device.updateDescriptorSets(DescriptorSetWrites, {});

    DeferredDestructionQueue.enqueue([this, GlobalLightDescriptorSet, GBufferSampler](){
        Device.freeDescriptorSets(TransientDescriptorPool, GlobalLightDescriptorSet);
        Device.destroySampler(GBufferSampler);
    });

    std::array<vk::ImageMemoryBarrier2, 3> GlobalLightImageBarriers{};
    GlobalLightImageBarriers[0]
            .setSrcStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
            .setSrcAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)
            .setDstStageMask(vk::PipelineStageFlagBits2::eComputeShader)
            .setDstAccessMask(vk::AccessFlagBits2::eShaderSampledRead)
            .setOldLayout(vk::ImageLayout::eColorAttachmentOptimal)
            .setNewLayout(vk::ImageLayout::eReadOnlyOptimalKHR)
            .setSubresourceRange(vkutil::flat_subresource_range(vk::ImageAspectFlagBits::eColor))
            .setImage(GBuffer.Normal.Image);

    GlobalLightImageBarriers[1]
            .setSrcStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
            .setSrcAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)
            .setDstStageMask(vk::PipelineStageFlagBits2::eComputeShader)
            .setDstAccessMask(vk::AccessFlagBits2::eShaderSampledRead)
            .setOldLayout(vk::ImageLayout::eColorAttachmentOptimal)
            .setNewLayout(vk::ImageLayout::eReadOnlyOptimalKHR)
            .setSubresourceRange(vkutil::flat_subresource_range(vk::ImageAspectFlagBits::eColor))
            .setImage(GBuffer.Color.Image);

    GlobalLightImageBarriers[2]
            .setSrcStageMask(vk::PipelineStageFlagBits2::eNone)
            .setSrcAccessMask(vk::AccessFlagBits2::eNone)
            .setDstStageMask(vk::PipelineStageFlagBits2::eComputeShader)
            .setDstAccessMask(vk::AccessFlagBits2::eShaderStorageWrite)
            .setOldLayout(vk::ImageLayout::eUndefined)
            .setNewLayout(vk::ImageLayout::eGeneral)
            .setSubresourceRange(vkutil::flat_subresource_range(vk::ImageAspectFlagBits::eColor))
            .setImage(Images[SwapChainImage]);

    auto GlobalLightDependencyInfo = vk::DependencyInfo{}
    .setImageMemoryBarriers(GlobalLightImageBarriers);

    ActiveFrame->CommandBuffer.pipelineBarrier2(GlobalLightDependencyInfo);

    VGlobalLightPC GlobalLightPushConstants{};
    GlobalLightPushConstants.lightDirection = axis::down;
    GlobalLightPushConstants.lightColor = glm::fvec3{1.0, 1.0, 1.0};
    GlobalLightPushConstants.lightStrength = 8.f;

    ActiveFrame->CommandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, GlobalLightPipeline);
    ActiveFrame->CommandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, GlobalLightPipelineLayout, 0, 1, &GlobalLightDescriptorSet, 0, nullptr);
    ActiveFrame->CommandBuffer.pushConstants(GlobalLightPipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(GlobalLightPushConstants), &GlobalLightPushConstants);
    ActiveFrame->CommandBuffer.dispatch(vkutil::GroupCount(ImageExtent.width, 8), vkutil::GroupCount(ImageExtent.height, 8), 1);

    auto SwapChainImage2PresentSrc = vk::ImageMemoryBarrier2{}
            .setSrcStageMask(vk::PipelineStageFlagBits2::eComputeShader)
            .setSrcAccessMask(vk::AccessFlagBits2::eShaderStorageWrite)
            .setDstStageMask(vk::PipelineStageFlagBits2::eAllCommands)
            .setDstAccessMask(vk::AccessFlagBits2::eNone)
            .setOldLayout(vk::ImageLayout::eGeneral)
            .setNewLayout(vk::ImageLayout::ePresentSrcKHR)
            .setSubresourceRange(vkutil::flat_subresource_range(vk::ImageAspectFlagBits::eColor))
            .setImage(Images[SwapChainImage]);

    auto SwapChainImage2PresentSrc_Dependency = vk::DependencyInfo{}
            .setImageMemoryBarriers(SwapChainImage2PresentSrc);

    ActiveFrame->CommandBuffer.pipelineBarrier2(SwapChainImage2PresentSrc_Dependency);

    ActiveFrame->CommandBuffer.end();

    auto CommandInfo = vk::CommandBufferSubmitInfo{}
            .setCommandBuffer(ActiveFrame->CommandBuffer);

    auto WaitInfo = vk::SemaphoreSubmitInfo{}
            .setSemaphore(ActiveFrame->ImageAvailable)
            .setStageMask(PipelineStage::eComputeShader);

    auto SignalInfo = vk::SemaphoreSubmitInfo{}
            .setSemaphore(ActiveFrame->DrawFinished)
            .setStageMask(PipelineStage::eComputeShader);

    auto SubmitInfo = vk::SubmitInfo2{}
            .setCommandBufferInfos(CommandInfo)
            .setWaitSemaphoreInfos(WaitInfo)
            .setSignalSemaphoreInfos(SignalInfo);

    QueueHandles.Graphics.submit2(SubmitInfo, ActiveFrame->InFlight);

    if(!PresentImage(SwapChainImage))
    {
        RecreateSwapChain();
    }
    else
    {
        AdvanceActiveFrame();
    }
}

void VStarSightRenderer::CreateGeometryPipeline()
{
    VGraphicsPipelineBuilder Builder = MakeGraphicsPipelineBuilder();

    Builder.IncludeShader(ProjectAbsolutePath("shaders/geometry.vert"));
    Builder.IncludeShader(ProjectAbsolutePath("shaders/geometry.frag"));

    std::array DynamicStates{vk::DynamicState::eViewport, vk::DynamicState::eScissor};

    Builder.dynamic_state
            .setDynamicStates(DynamicStates);

    Builder.viewport
         .setViewportCount(1)
         .setScissorCount(1);

    std::array ColorAttachmentFormats{GBuffer.PositionFormat, GBuffer.NormalFormat, GBuffer.ColorFormat};

    Builder.rendering
            .setColorAttachmentFormats(ColorAttachmentFormats)
            .setDepthAttachmentFormat(GBuffer.DepthFormat);

    Builder.input_assembly
            .setPrimitiveRestartEnable(false)
            .setTopology(vk::PrimitiveTopology::eTriangleList);

    Builder.rasterization
            .setFrontFace(vk::FrontFace::eCounterClockwise)
            .setCullMode(vk::CullModeFlagBits::eBack)
            .setPolygonMode(vk::PolygonMode::eFill)
            .setRasterizerDiscardEnable(false)
            .setDepthClampEnable(false)
            .setDepthBiasEnable(false)
            .setLineWidth(1.0);

    Builder.multisample
    .setSampleShadingEnable(false);

    constexpr vk::ColorComponentFlags AllColorComponents = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    std::array ColorBlendAttachments{
        vk::PipelineColorBlendAttachmentState{}
                .setBlendEnable(false)
                .setColorWriteMask(AllColorComponents),
        vk::PipelineColorBlendAttachmentState{}
                .setBlendEnable(false)
                .setColorWriteMask(AllColorComponents),
        vk::PipelineColorBlendAttachmentState{}
                .setBlendEnable(false)
                .setColorWriteMask(AllColorComponents),
    };

    Builder.color_blend
    .setLogicOpEnable(false)
    .setAttachments(ColorBlendAttachments);

    Builder.depth_stencil
            .setDepthTestEnable(true)
            .setDepthWriteEnable(true)
            .setDepthCompareOp(vk::CompareOp::eGreater)
            .setDepthBoundsTestEnable(true)
            .setMinDepthBounds(0.0)
            .setMaxDepthBounds(1.0)
            .setStencilTestEnable(false);

    Builder.Build(&GeometryPipelineLayout, &GeometryPipeline, "Geometry Pipeline");

    DestructionQueue.emplace_back([this](){
        Device.destroyPipeline(GeometryPipeline);
    });
}

void VStarSightRenderer::CreateGlobalLightPipeline()
{
    VComputePipelineBuilder Builder = MakeComputePipelineBuilder();
    Builder.IncludeShader(ProjectAbsolutePath("shaders/global_light.comp"));
    Builder.Build(&GlobalLightPipelineLayout, &GlobalLightPipeline, "Global Light Pipeline");

    DestructionQueue.emplace_back([this]{
        Device.destroyPipeline(GlobalLightPipeline);
    });
}


/*
void VRenderTarget::EndGeometryPass(uint32_t SwapChainImage)
{

}
*/
