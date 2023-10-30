#include "vk_context.hpp"
#include "core/log.hpp"
#include "core/assertion.hpp"
#include "fmt/format.h"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

static vk::Bool32 VulkanDebugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT type, vk::DebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data)
{
    std::string ObjectInfo = fmt::format("Objects: ({})\n", callback_data->objectCount);

    for(uint32_t object = 0; object < callback_data->objectCount; ++object)
    {
        auto& ObjectRef = callback_data->pObjects[object];

        ObjectInfo.append(fmt::format("Object[{}] = Type: {} | Handle: {} | Name: {}\n",
                                      object,
                                      vk::to_string(ObjectRef.objectType),
                                      ObjectRef.objectHandle,
                                      ObjectRef.pObjectName ? ObjectRef.pObjectName : "?"));
    }
    
    std::string CmdBufLabelInfo = fmt::format("Command Buffer Labels: ({})\n", callback_data->cmdBufLabelCount);

    for(uint32_t label = 0; label < callback_data->cmdBufLabelCount; ++label)
    {
        auto& LabelRef = callback_data->pCmdBufLabels[label];
        CmdBufLabelInfo.append(fmt::format("Label[{}] = {}\n", label, LabelRef.pLabelName));
    }
    
    std::string QueueLabelInfo = fmt::format("Queue Labels: ({})\n", callback_data->queueLabelCount);

    for(uint32_t label = 0; label < callback_data->queueLabelCount; ++label)
    {
        auto& LabelRef = callback_data->pQueueLabels[label];
        QueueLabelInfo.append(fmt::format("Label[{}] = {}\n", label, LabelRef.pLabelName));
    }
    
    std::string Message = fmt::format("Severity: {} | Type: {} | MessageIdNumber: {} | MessageIdName: {}\nMessage: {}\n{}{}{}",
                                      vk::to_string(severity),
                                      vk::to_string(type),
                                      callback_data->messageIdNumber,
                                      callback_data->pMessageIdName,
                                      callback_data->pMessage,
                                      ObjectInfo,
                                      CmdBufLabelInfo,
                                      QueueLabelInfo);

    switch(severity)
    {
        case vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose:
        case vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo:
            LOG_INFO("{}", Message);
            break;
        case vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning:
            LOG_WARNING("{}", Message);
            break;
        case vk::DebugUtilsMessageSeverityFlagBitsEXT::eError:
            LOG_ERROR("{}", Message);
            throw vk::ValidationFailedEXTError{"validation failure"};
    }

    return false;
}

static vk::DebugUtilsMessengerCreateInfoEXT MakeMessengerCreateInfo()
{
    using enum vk::DebugUtilsMessageSeverityFlagBitsEXT;
    using enum vk::DebugUtilsMessageTypeFlagBitsEXT;

    vk::DebugUtilsMessengerCreateInfoEXT messenger_info{};
    messenger_info.messageSeverity = eWarning | eError;
    messenger_info.messageType = eValidation | eGeneral | ePerformance | eDeviceAddressBinding;

    messenger_info.pfnUserCallback = reinterpret_cast<PFN_vkDebugUtilsMessengerCallbackEXT>(&::VulkanDebugCallback);
    return messenger_info;
}

static bool DeviceSupportsFeatures(vk::PhysicalDevice PhysicalDevice, VPhysicalDeviceFeatures Required)
{
    VPhysicalDeviceFeatures Available{};
    PhysicalDevice.getFeatures2(&Available.features2);

    bool supported = true;

    auto CheckSupported = [&supported](vk::Bool32 bRequired, vk::Bool32 bAvailable, std::string_view FeatureName)
    {
        if(bRequired && !bAvailable)
        {
            supported = false;
            LOG_WARNING("{} is required but not supported", FeatureName);
        }
    };

    CheckSupported(Required.features2.features.robustBufferAccess, Available.features2.features.robustBufferAccess, "robustBufferAccess");
    CheckSupported(Required.features2.features.fullDrawIndexUint32, Available.features2.features.fullDrawIndexUint32, "fullDrawIndexUint32");
    CheckSupported(Required.features2.features.imageCubeArray, Available.features2.features.imageCubeArray, "imageCubeArray");
    CheckSupported(Required.features2.features.independentBlend, Available.features2.features.independentBlend, "independentBlend");
    CheckSupported(Required.features2.features.geometryShader, Available.features2.features.geometryShader, "geometryShader");
    CheckSupported(Required.features2.features.tessellationShader, Available.features2.features.tessellationShader, "tessellationShader");
    CheckSupported(Required.features2.features.sampleRateShading, Available.features2.features.sampleRateShading, "sampleRateShading");
    CheckSupported(Required.features2.features.dualSrcBlend, Available.features2.features.dualSrcBlend, "dualSrcBlend");
    CheckSupported(Required.features2.features.logicOp, Available.features2.features.logicOp, "logicOp");
    CheckSupported(Required.features2.features.multiDrawIndirect, Available.features2.features.multiDrawIndirect, "multiDrawIndirect");
    CheckSupported(Required.features2.features.drawIndirectFirstInstance, Available.features2.features.drawIndirectFirstInstance, "drawIndirectFirstInstance");
    CheckSupported(Required.features2.features.depthClamp, Available.features2.features.depthClamp, "depthClamp");
    CheckSupported(Required.features2.features.depthBiasClamp, Available.features2.features.depthBiasClamp, "depthBiasClamp");
    CheckSupported(Required.features2.features.fillModeNonSolid, Available.features2.features.fillModeNonSolid, "fillModeNonSolid");
    CheckSupported(Required.features2.features.depthBounds, Available.features2.features.depthBounds, "depthBounds");
    CheckSupported(Required.features2.features.wideLines, Available.features2.features.wideLines, "wideLines");
    CheckSupported(Required.features2.features.largePoints, Available.features2.features.largePoints, "largePoints");
    CheckSupported(Required.features2.features.alphaToOne, Available.features2.features.alphaToOne, "alphaToOne");
    CheckSupported(Required.features2.features.multiViewport, Available.features2.features.multiViewport, "multiViewport");
    CheckSupported(Required.features2.features.samplerAnisotropy, Available.features2.features.samplerAnisotropy, "samplerAnisotropy");
    CheckSupported(Required.features2.features.textureCompressionETC2, Available.features2.features.textureCompressionETC2, "textureCompressionETC2");
    CheckSupported(Required.features2.features.textureCompressionASTC_LDR, Available.features2.features.textureCompressionASTC_LDR, "textureCompressionASTC_LDR");
    CheckSupported(Required.features2.features.textureCompressionBC, Available.features2.features.textureCompressionBC, "textureCompressionBC");
    CheckSupported(Required.features2.features.occlusionQueryPrecise, Available.features2.features.occlusionQueryPrecise, "occlusionQueryPrecise");
    CheckSupported(Required.features2.features.pipelineStatisticsQuery, Available.features2.features.pipelineStatisticsQuery, "pipelineStatisticsQuery");
    CheckSupported(Required.features2.features.vertexPipelineStoresAndAtomics, Available.features2.features.vertexPipelineStoresAndAtomics, "vertexPipelineStoresAndAtomics");
    CheckSupported(Required.features2.features.fragmentStoresAndAtomics, Available.features2.features.fragmentStoresAndAtomics, "fragmentStoresAndAtomics");
    CheckSupported(Required.features2.features.shaderTessellationAndGeometryPointSize, Available.features2.features.shaderTessellationAndGeometryPointSize, "shaderTessellationAndGeometryPointSize");
    CheckSupported(Required.features2.features.shaderImageGatherExtended, Available.features2.features.shaderImageGatherExtended, "shaderImageGatherExtended");
    CheckSupported(Required.features2.features.shaderStorageImageExtendedFormats, Available.features2.features.shaderStorageImageExtendedFormats, "shaderStorageImageExtendedFormats");
    CheckSupported(Required.features2.features.shaderStorageImageMultisample, Available.features2.features.shaderStorageImageMultisample, "shaderStorageImageMultisample");
    CheckSupported(Required.features2.features.shaderStorageImageReadWithoutFormat, Available.features2.features.shaderStorageImageReadWithoutFormat, "shaderStorageImageReadWithoutFormat");
    CheckSupported(Required.features2.features.shaderStorageImageWriteWithoutFormat, Available.features2.features.shaderStorageImageWriteWithoutFormat, "shaderStorageImageWriteWithoutFormat");
    CheckSupported(Required.features2.features.shaderUniformBufferArrayDynamicIndexing, Available.features2.features.shaderUniformBufferArrayDynamicIndexing, "shaderUniformBufferArrayDynamicIndexing");
    CheckSupported(Required.features2.features.shaderSampledImageArrayDynamicIndexing, Available.features2.features.shaderSampledImageArrayDynamicIndexing, "shaderSampledImageArrayDynamicIndexing");
    CheckSupported(Required.features2.features.shaderStorageBufferArrayDynamicIndexing, Available.features2.features.shaderStorageBufferArrayDynamicIndexing, "shaderStorageBufferArrayDynamicIndexing");
    CheckSupported(Required.features2.features.shaderStorageImageArrayDynamicIndexing, Available.features2.features.shaderStorageImageArrayDynamicIndexing, "shaderStorageImageArrayDynamicIndexing");
    CheckSupported(Required.features2.features.shaderClipDistance, Available.features2.features.shaderClipDistance, "shaderClipDistance");
    CheckSupported(Required.features2.features.shaderCullDistance, Available.features2.features.shaderCullDistance, "shaderCullDistance");
    CheckSupported(Required.features2.features.shaderFloat64, Available.features2.features.shaderFloat64, "shaderFloat64");
    CheckSupported(Required.features2.features.shaderInt64, Available.features2.features.shaderInt64, "shaderInt64");
    CheckSupported(Required.features2.features.shaderInt16, Available.features2.features.shaderInt16, "shaderInt16");
    CheckSupported(Required.features2.features.shaderResourceResidency, Available.features2.features.shaderResourceResidency, "shaderResourceResidency");
    CheckSupported(Required.features2.features.shaderResourceMinLod, Available.features2.features.shaderResourceMinLod, "shaderResourceMinLod");
    CheckSupported(Required.features2.features.sparseBinding, Available.features2.features.sparseBinding, "sparseBinding");
    CheckSupported(Required.features2.features.sparseResidencyBuffer, Available.features2.features.sparseResidencyBuffer, "sparseResidencyBuffer");
    CheckSupported(Required.features2.features.sparseResidencyImage2D, Available.features2.features.sparseResidencyImage2D, "sparseResidencyImage2D");
    CheckSupported(Required.features2.features.sparseResidencyImage3D, Available.features2.features.sparseResidencyImage3D, "sparseResidencyImage3D");
    CheckSupported(Required.features2.features.sparseResidency2Samples, Available.features2.features.sparseResidency2Samples, "sparseResidency2Samples");
    CheckSupported(Required.features2.features.sparseResidency4Samples, Available.features2.features.sparseResidency4Samples, "sparseResidency4Samples");
    CheckSupported(Required.features2.features.sparseResidency8Samples, Available.features2.features.sparseResidency8Samples, "sparseResidency8Samples");
    CheckSupported(Required.features2.features.sparseResidency16Samples, Available.features2.features.sparseResidency16Samples, "sparseResidency16Samples");
    CheckSupported(Required.features2.features.sparseResidencyAliased, Available.features2.features.sparseResidencyAliased, "sparseResidencyAliased");
    CheckSupported(Required.features2.features.variableMultisampleRate, Available.features2.features.variableMultisampleRate, "variableMultisampleRate");
    CheckSupported(Required.features2.features.inheritedQueries, Available.features2.features.inheritedQueries, "inheritedQueries");

    CheckSupported(Required.vk11features.storageBuffer16BitAccess, Available.vk11features.storageBuffer16BitAccess, "storageBuffer16BitAccess");
    CheckSupported(Required.vk11features.uniformAndStorageBuffer16BitAccess, Available.vk11features.uniformAndStorageBuffer16BitAccess, "uniformAndStorageBuffer16BitAccess");
    CheckSupported(Required.vk11features.storagePushConstant16, Available.vk11features.storagePushConstant16, "storagePushConstant16");
    CheckSupported(Required.vk11features.storageInputOutput16, Available.vk11features.storageInputOutput16, "storageInputOutput16");
    CheckSupported(Required.vk11features.multiview, Available.vk11features.multiview, "multiview");
    CheckSupported(Required.vk11features.multiviewGeometryShader, Available.vk11features.multiviewGeometryShader, "multiviewGeometryShader");
    CheckSupported(Required.vk11features.multiviewTessellationShader, Available.vk11features.multiviewTessellationShader, "multiviewTessellationShader");
    CheckSupported(Required.vk11features.variablePointersStorageBuffer, Available.vk11features.variablePointersStorageBuffer, "variablePointersStorageBuffer");
    CheckSupported(Required.vk11features.variablePointers, Available.vk11features.variablePointers, "variablePointers");
    CheckSupported(Required.vk11features.protectedMemory, Available.vk11features.protectedMemory, "protectedMemory");
    CheckSupported(Required.vk11features.samplerYcbcrConversion, Available.vk11features.samplerYcbcrConversion, "samplerYcbcrConversion");
    CheckSupported(Required.vk11features.shaderDrawParameters, Available.vk11features.shaderDrawParameters, "shaderDrawParameters");

    CheckSupported(Required.vk12features.samplerMirrorClampToEdge, Available.vk12features.samplerMirrorClampToEdge, "samplerMirrorClampToEdge");
    CheckSupported(Required.vk12features.drawIndirectCount, Available.vk12features.drawIndirectCount, "drawIndirectCount");
    CheckSupported(Required.vk12features.storageBuffer8BitAccess, Available.vk12features.storageBuffer8BitAccess, "storageBuffer8BitAccess");
    CheckSupported(Required.vk12features.uniformAndStorageBuffer8BitAccess, Available.vk12features.uniformAndStorageBuffer8BitAccess, "uniformAndStorageBuffer8BitAccess");
    CheckSupported(Required.vk12features.storagePushConstant8, Available.vk12features.storagePushConstant8, "storagePushConstant8");
    CheckSupported(Required.vk12features.shaderBufferInt64Atomics, Available.vk12features.shaderBufferInt64Atomics, "shaderBufferInt64Atomics");
    CheckSupported(Required.vk12features.shaderSharedInt64Atomics, Available.vk12features.shaderSharedInt64Atomics, "shaderSharedInt64Atomics");
    CheckSupported(Required.vk12features.shaderFloat16, Available.vk12features.shaderFloat16, "shaderFloat16");
    CheckSupported(Required.vk12features.shaderInt8, Available.vk12features.shaderInt8, "shaderInt8");
    CheckSupported(Required.vk12features.descriptorIndexing, Available.vk12features.descriptorIndexing, "descriptorIndexing");
    CheckSupported(Required.vk12features.shaderInputAttachmentArrayDynamicIndexing, Available.vk12features.shaderInputAttachmentArrayDynamicIndexing, "shaderInputAttachmentArrayDynamicIndexing");
    CheckSupported(Required.vk12features.shaderUniformTexelBufferArrayDynamicIndexing, Available.vk12features.shaderUniformTexelBufferArrayDynamicIndexing, "shaderUniformTexelBufferArrayDynamicIndexing");
    CheckSupported(Required.vk12features.shaderStorageTexelBufferArrayDynamicIndexing, Available.vk12features.shaderStorageTexelBufferArrayDynamicIndexing, "shaderStorageTexelBufferArrayDynamicIndexing");
    CheckSupported(Required.vk12features.shaderUniformBufferArrayNonUniformIndexing, Available.vk12features.shaderUniformBufferArrayNonUniformIndexing, "shaderUniformBufferArrayNonUniformIndexing");
    CheckSupported(Required.vk12features.shaderSampledImageArrayNonUniformIndexing, Available.vk12features.shaderSampledImageArrayNonUniformIndexing, "shaderSampledImageArrayNonUniformIndexing");
    CheckSupported(Required.vk12features.shaderStorageBufferArrayNonUniformIndexing, Available.vk12features.shaderStorageBufferArrayNonUniformIndexing, "shaderStorageBufferArrayNonUniformIndexing");
    CheckSupported(Required.vk12features.shaderStorageImageArrayNonUniformIndexing, Available.vk12features.shaderStorageImageArrayNonUniformIndexing, "shaderStorageImageArrayNonUniformIndexing");
    CheckSupported(Required.vk12features.shaderInputAttachmentArrayNonUniformIndexing, Available.vk12features.shaderInputAttachmentArrayNonUniformIndexing, "shaderInputAttachmentArrayNonUniformIndexing");
    CheckSupported(Required.vk12features.shaderUniformTexelBufferArrayNonUniformIndexing, Available.vk12features.shaderUniformTexelBufferArrayNonUniformIndexing, "shaderUniformTexelBufferArrayNonUniformIndexing");
    CheckSupported(Required.vk12features.shaderStorageTexelBufferArrayNonUniformIndexing, Available.vk12features.shaderStorageTexelBufferArrayNonUniformIndexing, "shaderStorageTexelBufferArrayNonUniformIndexing");
    CheckSupported(Required.vk12features.descriptorBindingUniformBufferUpdateAfterBind, Available.vk12features.descriptorBindingUniformBufferUpdateAfterBind, "descriptorBindingUniformBufferUpdateAfterBind");
    CheckSupported(Required.vk12features.descriptorBindingSampledImageUpdateAfterBind, Available.vk12features.descriptorBindingSampledImageUpdateAfterBind, "descriptorBindingSampledImageUpdateAfterBind");
    CheckSupported(Required.vk12features.descriptorBindingStorageImageUpdateAfterBind, Available.vk12features.descriptorBindingStorageImageUpdateAfterBind, "descriptorBindingStorageImageUpdateAfterBind");
    CheckSupported(Required.vk12features.descriptorBindingStorageBufferUpdateAfterBind, Available.vk12features.descriptorBindingStorageBufferUpdateAfterBind, "descriptorBindingStorageBufferUpdateAfterBind");
    CheckSupported(Required.vk12features.descriptorBindingUniformTexelBufferUpdateAfterBind, Available.vk12features.descriptorBindingUniformTexelBufferUpdateAfterBind, "descriptorBindingUniformTexelBufferUpdateAfterBind");
    CheckSupported(Required.vk12features.descriptorBindingStorageTexelBufferUpdateAfterBind, Available.vk12features.descriptorBindingStorageTexelBufferUpdateAfterBind, "descriptorBindingStorageTexelBufferUpdateAfterBind");
    CheckSupported(Required.vk12features.descriptorBindingUpdateUnusedWhilePending, Available.vk12features.descriptorBindingUpdateUnusedWhilePending, "descriptorBindingUpdateUnusedWhilePending");
    CheckSupported(Required.vk12features.descriptorBindingPartiallyBound, Available.vk12features.descriptorBindingPartiallyBound, "descriptorBindingPartiallyBound");
    CheckSupported(Required.vk12features.descriptorBindingVariableDescriptorCount, Available.vk12features.descriptorBindingVariableDescriptorCount, "descriptorBindingVariableDescriptorCount");
    CheckSupported(Required.vk12features.runtimeDescriptorArray, Available.vk12features.runtimeDescriptorArray, "runtimeDescriptorArray");
    CheckSupported(Required.vk12features.samplerFilterMinmax, Available.vk12features.samplerFilterMinmax, "samplerFilterMinmax");
    CheckSupported(Required.vk12features.scalarBlockLayout, Available.vk12features.scalarBlockLayout, "scalarBlockLayout");
    CheckSupported(Required.vk12features.imagelessFramebuffer, Available.vk12features.imagelessFramebuffer, "imagelessFramebuffer");
    CheckSupported(Required.vk12features.uniformBufferStandardLayout, Available.vk12features.uniformBufferStandardLayout, "uniformBufferStandardLayout");
    CheckSupported(Required.vk12features.shaderSubgroupExtendedTypes, Available.vk12features.shaderSubgroupExtendedTypes, "shaderSubgroupExtendedTypes");
    CheckSupported(Required.vk12features.separateDepthStencilLayouts, Available.vk12features.separateDepthStencilLayouts, "separateDepthStencilLayouts");
    CheckSupported(Required.vk12features.hostQueryReset, Available.vk12features.hostQueryReset, "hostQueryReset");
    CheckSupported(Required.vk12features.timelineSemaphore, Available.vk12features.timelineSemaphore, "timelineSemaphore");
    CheckSupported(Required.vk12features.bufferDeviceAddress, Available.vk12features.bufferDeviceAddress, "bufferDeviceAddress");
    CheckSupported(Required.vk12features.bufferDeviceAddressCaptureReplay, Available.vk12features.bufferDeviceAddressCaptureReplay, "bufferDeviceAddressCaptureReplay");
    CheckSupported(Required.vk12features.bufferDeviceAddressMultiDevice, Available.vk12features.bufferDeviceAddressMultiDevice, "bufferDeviceAddressMultiDevice");
    CheckSupported(Required.vk12features.vulkanMemoryModel, Available.vk12features.vulkanMemoryModel, "vulkanMemoryModel");
    CheckSupported(Required.vk12features.vulkanMemoryModelDeviceScope, Available.vk12features.vulkanMemoryModelDeviceScope, "vulkanMemoryModelDeviceScope");
    CheckSupported(Required.vk12features.vulkanMemoryModelAvailabilityVisibilityChains, Available.vk12features.vulkanMemoryModelAvailabilityVisibilityChains, "vulkanMemoryModelAvailabilityVisibilityChains");
    CheckSupported(Required.vk12features.shaderOutputViewportIndex, Available.vk12features.shaderOutputViewportIndex, "shaderOutputViewportIndex");
    CheckSupported(Required.vk12features.shaderOutputLayer, Available.vk12features.shaderOutputLayer, "shaderOutputLayer");
    CheckSupported(Required.vk12features.subgroupBroadcastDynamicId, Available.vk12features.subgroupBroadcastDynamicId, "subgroupBroadcastDynamicId");

    CheckSupported(Required.vk13features.robustImageAccess, Available.vk13features.robustImageAccess, "robustImageAccess");
    CheckSupported(Required.vk13features.inlineUniformBlock, Available.vk13features.inlineUniformBlock, "inlineUniformBlock");
    CheckSupported(Required.vk13features.descriptorBindingInlineUniformBlockUpdateAfterBind, Available.vk13features.descriptorBindingInlineUniformBlockUpdateAfterBind, "descriptorBindingInlineUniformBlockUpdateAfterBind");
    CheckSupported(Required.vk13features.pipelineCreationCacheControl, Available.vk13features.pipelineCreationCacheControl, "pipelineCreationCacheControl");
    CheckSupported(Required.vk13features.privateData, Available.vk13features.privateData, "privateData");
    CheckSupported(Required.vk13features.shaderDemoteToHelperInvocation, Available.vk13features.shaderDemoteToHelperInvocation, "shaderDemoteToHelperInvocation");
    CheckSupported(Required.vk13features.shaderTerminateInvocation, Available.vk13features.shaderTerminateInvocation, "shaderTerminateInvocation");
    CheckSupported(Required.vk13features.subgroupSizeControl, Available.vk13features.subgroupSizeControl, "subgroupSizeControl");
    CheckSupported(Required.vk13features.computeFullSubgroups, Available.vk13features.computeFullSubgroups, "computeFullSubgroups");
    CheckSupported(Required.vk13features.synchronization2, Available.vk13features.synchronization2, "synchronization2");
    CheckSupported(Required.vk13features.textureCompressionASTC_HDR, Available.vk13features.textureCompressionASTC_HDR, "textureCompressionASTC_HDR");
    CheckSupported(Required.vk13features.shaderZeroInitializeWorkgroupMemory, Available.vk13features.shaderZeroInitializeWorkgroupMemory, "shaderZeroInitializeWorkgroupMemory");
    CheckSupported(Required.vk13features.dynamicRendering, Available.vk13features.dynamicRendering, "dynamicRendering");
    CheckSupported(Required.vk13features.shaderIntegerDotProduct, Available.vk13features.shaderIntegerDotProduct, "shaderIntegerDotProduct");
    CheckSupported(Required.vk13features.maintenance4, Available.vk13features.maintenance4, "maintenance4");

    return supported;
}

static bool DeviceSupportsExtensions(vk::PhysicalDevice PhysicalDevice, const std::vector<const char*>& DeviceExtensions)
{
    bool supported = true;
    for(const char* logical_device_extension : DeviceExtensions)
    {
        bool extension_supported = false;
        for(vk::ExtensionProperties& extension_property : PhysicalDevice.enumerateDeviceExtensionProperties())
        {
            if(strcmp(logical_device_extension, extension_property.extensionName) == 0)
            {
                extension_supported = true;
                break;
            }
        }

        if(!extension_supported)
        {
            supported = false;
            LOG_INFO("{} not supported", logical_device_extension);
        }
    }

    return supported;
}

static VQueueIndices GetDeviceQueues(vk::PhysicalDevice PhysicalDevice)
{
    std::vector<vk::QueueFamilyProperties> queue_properties = PhysicalDevice.getQueueFamilyProperties();
    VQueueIndices Queues{};

    for(uint32_t queue_index = 0; queue_index < queue_properties.size(); ++queue_index)
    {
        vk::QueueFlags flags = queue_properties[queue_index].queueFlags;

        if(Queues.Present == -1 || Queues.Present != Queues.Graphics)
        {
            if(flags & vk::QueueFlagBits::eGraphics) //we are just assuming that graphics queue will always support presentation
            {
                Queues.Graphics = queue_index;
                Queues.Present = queue_index;
            }
        }

        if(Queues.Transfer == -1 || Queues.Transfer == Queues.Present || Queues.Transfer == Queues.Graphics)
        {
            if(flags & vk::QueueFlagBits::eTransfer)
            {
                Queues.Transfer = queue_index;
            }
        }

        if(Queues.Compute == -1 || Queues.Compute == Queues.Present || Queues.Compute == Queues.Graphics)
        {
            if(flags & vk::QueueFlagBits::eCompute)
            {
                Queues.Compute = queue_index;
            }
        }
    }

    return Queues;
}

VContext::VContext()
{
    LOG_INFO("creating vulkan context");

    ASSERT(vkContext == nullptr);
    vkContext = this;

    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

    InstanceExtensions.emplace_back(VK_KHR_SURFACE_EXTENSION_NAME);
    InstanceExtensions.emplace_back("VK_KHR_xcb_surface");

    DeviceExtensions.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    DeviceExtensions.emplace_back(VK_EXT_CONDITIONAL_RENDERING_EXTENSION_NAME);

#ifndef NDEBUG
    ValidationLayers.emplace_back("VK_LAYER_KHRONOS_validation");
    InstanceExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

    PhysicalDeviceFeatures.features2.features.samplerAnisotropy = true;
    PhysicalDeviceFeatures.vk12features.timelineSemaphore = true;
    PhysicalDeviceFeatures.vk12features.scalarBlockLayout = true;
    PhysicalDeviceFeatures.vk13features.synchronization2 = true;
    PhysicalDeviceFeatures.vk13features.dynamicRendering = true;
    PhysicalDeviceFeatures.vk13features.maintenance4 = true;

    CreateInstance();
    CreateDebugMessenger();
    PickPhysicalDevice();
    CreateLogicalDevice();
    CreateMemoryAllocator();
    CreateCommandPool();
    CreateCaches();
}

VContext::~VContext()
{
    LOG_INFO("destroying vulkan context");

    if(Device)
    {
        Device.waitIdle();
    }

    for(auto it = DestructionQueue.rbegin(); it != DestructionQueue.rend(); ++it)
    {
        (*it)();
    }
}

void VContext::CreateInstance()
{
    LOG_INFO("creating instance");

    vk::ApplicationInfo app_info{};
    app_info.pApplicationName = "rendered background";
    app_info.pEngineName = "none";
    app_info.applicationVersion = VK_MAKE_API_VERSION(1, 0, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_3;

    auto instance_info = vk::InstanceCreateInfo{}
            .setPApplicationInfo(&app_info)
            .setPEnabledLayerNames(ValidationLayers)
            .setPEnabledExtensionNames(InstanceExtensions);

    for(const char* name : ValidationLayers)
    {
        LOG_INFO("using validation layer {}", name);
    }

    for(const char* name : InstanceExtensions)
    {
        LOG_INFO("using instance extension {}", name);
    }

#ifndef NDEBUG
    vk::DebugUtilsMessengerCreateInfoEXT messenger_info = MakeMessengerCreateInfo();
    instance_info.pNext = &messenger_info;
#else
    instance_info.pNext = nullptr;
#endif

    Instance = vk::createInstance(instance_info, nullptr);

    DestructionQueue.emplace_back([this]()
                                  {
                                      Instance.destroy();
                                  });

    VULKAN_HPP_DEFAULT_DISPATCHER.init(Instance);
}

void VContext::CreateDebugMessenger()
{
#ifndef NDEBUG
    LOG_INFO("creating debug messenger");

    vk::DebugUtilsMessengerCreateInfoEXT messenger_info = MakeMessengerCreateInfo();
    DebugMessenger = Instance.createDebugUtilsMessengerEXT(messenger_info);

    DestructionQueue.emplace_back([this]()
                                  {
                                      Instance.destroyDebugUtilsMessengerEXT(DebugMessenger);
                                  });
#endif
}

void VContext::PickPhysicalDevice()
{
    LOG_INFO("picking physical device");

    for(vk::PhysicalDevice pdevice : Instance.enumeratePhysicalDevices())
    {
        PhysicalDeviceProperties = pdevice.getProperties2();

        LOG_INFO("checking {}", std::string_view{PhysicalDeviceProperties.properties.deviceName});

        if(PhysicalDeviceProperties.properties.apiVersion < VK_API_VERSION_1_3)
        {
            LOG_INFO("does not support properties");
            continue;
        }

        if(!DeviceSupportsFeatures(pdevice, PhysicalDeviceFeatures))
        {
            LOG_INFO("does not support features");
            continue;
        }

        if(!DeviceSupportsExtensions(pdevice, DeviceExtensions))
        {
            LOG_INFO("does not support extensions");
            continue;
        }

        QueueIndices = GetDeviceQueues(pdevice);

        if(QueueIndices.Graphics == UINT32_MAX || QueueIndices.Present == UINT32_MAX || QueueIndices.Transfer == UINT32_MAX)
        {
            LOG_INFO("does not support queues");
            continue;
        }

        PhysicalDevice = pdevice;

        if(PhysicalDeviceProperties.properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
        {
            break;
        }
    }

    VERIFY(!!PhysicalDevice);
}

void VContext::CreateLogicalDevice()
{
    LOG_INFO("creating logical device");

    float priority = 1.f;

    std::array queue_indices_arr{QueueIndices.Present, QueueIndices.Graphics, QueueIndices.Compute, QueueIndices.Transfer};
    std::vector<vk::DeviceQueueCreateInfo> queue_infos{};

    vk::DeviceQueueCreateInfo queue_create_info{};
    queue_create_info.queueCount = 1;
    queue_create_info.pQueuePriorities = &priority;

    for(uint32_t queue_index : queue_indices_arr)
    {
        for(vk::DeviceQueueCreateInfo queue_info : queue_infos)
        {
            if(queue_info.queueFamilyIndex == queue_index) //remove duplicates
            {
                goto duplicate;
            }
        }

        queue_create_info.queueFamilyIndex = queue_index;
        queue_infos.emplace_back(queue_create_info);

duplicate:;
    }

    for(const char* name: DeviceExtensions)
    {
        LOG_INFO("using device extension {}", name);
    }

    vk::PhysicalDeviceShaderAtomicFloat2FeaturesEXT shader_atomics2{};
    shader_atomics2.setShaderBufferFloat32AtomicMinMax(true);

    vk::DeviceCreateInfo device_info{};
    device_info.pNext = &PhysicalDeviceFeatures.features2;
    device_info.pQueueCreateInfos = queue_infos.data();
    device_info.queueCreateInfoCount = queue_infos.size();
    device_info.pEnabledFeatures = nullptr;
    device_info.ppEnabledExtensionNames = DeviceExtensions.data();
    device_info.enabledExtensionCount = DeviceExtensions.size();

    Device = PhysicalDevice.createDevice(device_info);

    NameObject(Device, "logical device");

    DestructionQueue.emplace_back([this]()
                                  {
                                      Device.destroy();
                                  });

    VULKAN_HPP_DEFAULT_DISPATCHER.init(Device);

    QueueHandles.Graphics = Device.getQueue(QueueIndices.Graphics, 0);
    QueueHandles.Present = Device.getQueue(QueueIndices.Present, 0);
    QueueHandles.Transfer = Device.getQueue(QueueIndices.Transfer, 0);
    QueueHandles.Compute = Device.getQueue(QueueIndices.Compute, 0);

    NameObject(QueueHandles.Graphics, "graphics queue");
    NameObject(QueueHandles.Present, "presentation queue");
    NameObject(QueueHandles.Transfer, "transfer queue");
    NameObject(QueueHandles.Compute, "compute queue");
}

void VContext::CreateMemoryAllocator()
{
    LOG_INFO("creating vulkan memory allocator");

    auto allocator_info = vma::AllocatorCreateInfo{}
            .setVulkanApiVersion(VK_API_VERSION_1_3)
            .setInstance(Instance)
            .setPhysicalDevice(PhysicalDevice)
            .setDevice(Device);

    Allocator = vma::createAllocator(allocator_info);

    DestructionQueue.emplace_back([this]()
                                  {
                                      Allocator.destroy();
                                  });
}

void VContext::CreateCaches()
{
    LOG_INFO("initializing caches");
/*
    DescriptorAllocator.Connect(Device);
    DescriptorLayoutCache.Connect(Device);
    PipelineLayoutCache.Connect(Device);
    ShaderModuleCache.Connect(Device);
    TransferManager.Initialize();

    DestructionQueue.emplace_back([this]()
                                  {
                                      TransferManager.Destroy();
                                      DescriptorAllocator.shutdown();
                                      DescriptorLayoutCache.shutdown();
                                      PipelineLayoutCache.shutdown();
                                      ShaderModuleCache.Destroy();
                                  });
    */
}

void VContext::CreateCommandPool()
{
    LOG_INFO("creating command pool");

    auto GraphicsPoolInfo = vk::CommandPoolCreateInfo{}
            .setQueueFamilyIndex(QueueIndices.Graphics)
            .setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);

    GraphicsCommandPool = Device.createCommandPool(GraphicsPoolInfo);

    NameObject(GraphicsCommandPool, "graphics command pool");
    DestructionQueue.emplace_back([this]()
                                  {
                                      Device.destroyCommandPool(GraphicsCommandPool);
                                  });

    auto TransferPoolInfo = vk::CommandPoolCreateInfo{}
            .setQueueFamilyIndex(QueueIndices.Transfer)
            .setFlags(vk::CommandPoolCreateFlagBits::eTransient);

    TransferCommandPool = Device.createCommandPool(TransferPoolInfo);

    NameObject(TransferCommandPool, "transfer command pool");
    DestructionQueue.emplace_back([this]()
                                  {
                                      Device.destroyCommandPool(TransferCommandPool);
                                  });
}
/*
VGraphicsPipelineBuilder VContext::MakeGraphicsPipelineBuilder()
{
    return VGraphicsPipelineBuilder{&DescriptorLayoutCache, &PipelineLayoutCache, &ShaderModuleCache};
}

VDescriptorBuilder VContext::MakeDescriptorBuilder()
{
    return VDescriptorBuilder{&DescriptorAllocator, &DescriptorLayoutCache};
}
*/
VAllocatedBuffer VContext::AllocateStagingBuffer(uint64_t Size)
{
    auto BufferCreatInfo = vk::BufferCreateInfo{}
            .setSize(Size)
            .setUsage(vk::BufferUsageFlagBits::eTransferSrc);

    auto AllocationCreateInfo = vma::AllocationCreateInfo{}
            .setFlags(vma::AllocationCreateFlagBits::eHostAccessRandom | vma::AllocationCreateFlagBits::eMapped | vma::AllocationCreateFlagBits::eStrategyMinTime)
            .setUsage(vma::MemoryUsage::eAutoPreferHost);

    VAllocatedBuffer Buffer{};
    resultcheck = Allocator.createBuffer(&BufferCreatInfo, &AllocationCreateInfo, &Buffer.Buffer, &Buffer.Allocation, &Buffer.Info);

    return Buffer;
}

VAllocatedBuffer VContext::AllocateVertexBuffer(uint64_t Size)
{
    auto BufferCreatInfo = vk::BufferCreateInfo{}
            .setSize(Size)
            .setUsage(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst);

    auto AllocationCreateInfo = vma::AllocationCreateInfo{}
            .setFlags(vma::AllocationCreateFlagBits::eStrategyMinMemory)
            .setUsage(vma::MemoryUsage::eAutoPreferDevice);

    VAllocatedBuffer Buffer{};
    resultcheck = Allocator.createBuffer(&BufferCreatInfo, &AllocationCreateInfo, &Buffer.Buffer, &Buffer.Allocation, &Buffer.Info);

    return Buffer;
}

vk::CommandBuffer VContext::RequestTransferCommandBuffer(std::string Description)
{
    TransferMutex.lock();

    auto CommandBufferAllocateInfo = vk::CommandBufferAllocateInfo{}
            .setCommandPool(TransferCommandPool)
            .setLevel(vk::CommandBufferLevel::ePrimary)
            .setCommandBufferCount(1);

    vk::CommandBuffer CommandBuffer = nullptr;
    resultcheck = Device.allocateCommandBuffers(&CommandBufferAllocateInfo, &CommandBuffer);

    NameObject(CommandBuffer, Description);
    vkutil::push_label(QueueHandles.Transfer, Description);

    auto CommandBeginInfo = vk::CommandBufferBeginInfo{}
            .setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    CommandBuffer.begin(CommandBeginInfo);
    return CommandBuffer;
}

void VContext::SubmitTransferCommands(vk::CommandBuffer Commands, vk::SubmitInfo2 Submits)
{
    Commands.end();
    QueueHandles.Transfer.submit2(Submits);

    vkutil::pop_label(QueueHandles.Transfer);
    TransferMutex.unlock();
}

void VContext::FreeTransferCommandBuffer(vk::CommandBuffer Commands)
{
    TransferMutex.lock();
    Device.freeCommandBuffers(TransferCommandPool, Commands);
    TransferMutex.unlock();
}

vk::Format VContext::PickImageFormat(vk::FormatFeatureFlags feature_flags, std::span<vk::Format> candidates) const
{
    for(vk::Format format : candidates)
    {
        vk::FormatProperties2 properties = PhysicalDevice.getFormatProperties2(format);

        if((properties.formatProperties.optimalTilingFeatures & feature_flags) == feature_flags)
        {
            return format;
        }
    }

    VERIFY(false, "no viable image format found");
}

vk::Format VContext::PickBufferFormat(vk::FormatFeatureFlags feature_flags, std::span<vk::Format> candidates) const
{
    for(vk::Format format : candidates)
    {
        vk::FormatProperties2 properties = PhysicalDevice.getFormatProperties2(format);

        if((properties.formatProperties.bufferFeatures & feature_flags) == feature_flags)
        {
            return format;
        }
    }

    VERIFY(false, "no viable buffer format found");
}

void VContext::NameObject(uint64_t Handle, vk::ObjectType Type, std::string Name)
{
#ifndef NDEBUG
    vk::DebugUtilsObjectNameInfoEXT info{};
    info.objectHandle = Handle;
    info.objectType = Type;
    info.pObjectName = Name.c_str();

    Device.setDebugUtilsObjectNameEXT(info);
#endif
}
