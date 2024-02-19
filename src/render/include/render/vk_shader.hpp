#ifndef STARSIGHT_VK_SHADER_HPP
#define STARSIGHT_VK_SHADER_HPP

#include "core/ssovector.hpp"
#include "spirv_reflect.hpp"
#include "shaderc/shaderc.hpp"
#include "taskflow/taskflow.hpp"
#include "tbb/concurrent_unordered_map.h"
#include "core/filesystem.hpp"
#include <vulkan/vulkan.hpp>
#include <string_view>
#include <span>
#include <unordered_map>
#include <future>

class VContext;

struct SpvResultCheck
{
    SpvResultCheck& operator=(SpvReflectResult Result_);
    operator SpvReflectResult() const {return Result;}
    SpvReflectResult Result{};
};
inline constinit thread_local SpvResultCheck spvResultCheck{};

struct VShader
{
    vk::ShaderModule ShaderModule = nullptr;
    SpvReflectShaderModule ReflectionModule{};
};

class VShaderCache
{
public:
    tbb::concurrent_unordered_map<std::fpath, VShader> Shaders{};

    tf::Executor TaskExecutor{};
    VContext* Context = nullptr;
public:

    explicit VShaderCache(VContext* Context_);
    ~VShaderCache();

    std::future<VShader*> CreateShader(const std::fpath& Path);
    bool DestroyShader(const std::fpath& Path);

private:

    VShader* CreateShader_Impl(VShader* Shader, std::fpath Path, bool IsSpirv);
};

#endif //STARSIGHT_VK_SHADER_HPP
