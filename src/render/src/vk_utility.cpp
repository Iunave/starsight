#include "vk_utility.hpp"
#include "core/assertion.hpp"

result_checker_t& result_checker_t::operator=(vk::Result val)
{
    VERIFY(val == vk::Result::eSuccess, vk::to_string(val));
    Value = val;
    return *this;
}

result_checker_t& result_checker_t::operator=(VkResult val)
{
    return *this = static_cast<vk::Result>(val);
}

void vkutil::push_label(vk::Queue queue, std::string label_name, std::array<float, 4> color)
{
#ifndef NDEBUG
    auto label_info = vk::DebugUtilsLabelEXT{}
            .setColor(color)
            .setPLabelName(label_name.c_str());

    queue.beginDebugUtilsLabelEXT(label_info);
#endif
}

void vkutil::insert_label(vk::Queue queue, std::string label_name, std::array<float, 4> color)
{
#ifndef NDEBUG
    auto label_info = vk::DebugUtilsLabelEXT{}
            .setColor(color)
            .setPLabelName(label_name.c_str());

    queue.insertDebugUtilsLabelEXT(label_info);
#endif
}

void vkutil::pop_label(vk::Queue queue)
{
#ifndef NDEBUG
    queue.endDebugUtilsLabelEXT();
#endif
}

void vkutil::push_label(vk::CommandBuffer cmdbuf, std::string label_name, std::array<float, 4> color)
{
#ifndef NDEBUG
    auto label_info = vk::DebugUtilsLabelEXT{}
            .setColor(color)
            .setPLabelName(label_name.c_str());

    cmdbuf.beginDebugUtilsLabelEXT(label_info);
#endif
}

void vkutil::insert_label(vk::CommandBuffer cmdbuf, std::string label_name, std::array<float, 4> color)
{
#ifndef NDEBUG
    auto label_info = vk::DebugUtilsLabelEXT{}
            .setColor(color)
            .setPLabelName(label_name.c_str());

    cmdbuf.insertDebugUtilsLabelEXT(label_info);
#endif
}

void vkutil::pop_label(vk::CommandBuffer cmdbuf)
{
#ifndef NDEBUG
    cmdbuf.endDebugUtilsLabelEXT();
#endif
}

vk::ImageSubresourceLayers vkutil::flat_subresource_layers(vk::ImageAspectFlagBits aspect)
{
    return vk::ImageSubresourceLayers{}
            .setAspectMask(aspect)
            .setLayerCount(1)
            .setBaseArrayLayer(0)
            .setMipLevel(0);
}

vk::ImageSubresourceRange vkutil::flat_subresource_range(vk::ImageAspectFlagBits aspect)
{
    return vk::ImageSubresourceRange{}
            .setAspectMask(aspect)
            .setLevelCount(1)
            .setLayerCount(1)
            .setBaseArrayLayer(0)
            .setBaseMipLevel(0);
}

uint32_t vkutil::groupcount(uint32_t total_size, uint32_t local_size)
{
    uint32_t count = total_size / local_size;
    uint32_t remainder = total_size % local_size;
    return remainder != 0 ? count + 1 : count;
}