#include "vk_shader.hpp"
#include "vk_utility.hpp"
#include "core/assertion.hpp"
#include "core/log.hpp"
#include "core/filesystem.hpp"
#include "libshaderc_util/file_finder.h"
#include "shaderc/glslc/src/file_includer.h"
#include "vk_context.hpp"

namespace
{
    vk::ShaderStageFlagBits ShaderName2ShaderStage(const std::fpath& ShaderName)
    {
        if(ShaderName.extension() == ".vert") return vk::ShaderStageFlagBits::eVertex;
        if(ShaderName.extension() == ".tesc") return vk::ShaderStageFlagBits::eTessellationControl;
        if(ShaderName.extension() == ".tese") return vk::ShaderStageFlagBits::eTessellationEvaluation;
        if(ShaderName.extension() == ".geom") return vk::ShaderStageFlagBits::eGeometry;
        if(ShaderName.extension() == ".frag") return vk::ShaderStageFlagBits::eFragment;
        if(ShaderName.extension() == ".comp") return vk::ShaderStageFlagBits::eCompute;
        VERIFY(false, ShaderName); __builtin_unreachable();
    }

    shaderc_shader_kind ShaderStage2ShaderKind(vk::ShaderStageFlagBits ShaderStage)
    {
        switch(ShaderStage)
        {
            case vk::ShaderStageFlagBits::eVertex: return shaderc_vertex_shader;
            case vk::ShaderStageFlagBits::eTessellationControl: return shaderc_tess_control_shader;
            case vk::ShaderStageFlagBits::eTessellationEvaluation: return shaderc_tess_evaluation_shader;
            case vk::ShaderStageFlagBits::eGeometry: return shaderc_geometry_shader;
            case vk::ShaderStageFlagBits::eFragment: return shaderc_fragment_shader;
            case vk::ShaderStageFlagBits::eCompute: return shaderc_compute_shader;
            default: VERIFY(false, vk::to_string(ShaderStage)); __builtin_unreachable();
        }
    }

    shaderc_shader_kind ShaderName2ShaderKind(const std::fpath& ShaderName)
    {
        return ShaderStage2ShaderKind(ShaderName2ShaderStage(ShaderName));
    }

    vk::ShaderStageFlagBits ShaderKind2ShaderStage(shaderc_shader_kind ShaderKind)
    {
        switch(ShaderKind)
        {
            case shaderc_vertex_shader: return vk::ShaderStageFlagBits::eVertex;
            case shaderc_tess_control_shader: return vk::ShaderStageFlagBits::eTessellationControl;
            case shaderc_tess_evaluation_shader: return vk::ShaderStageFlagBits::eTessellationEvaluation;
            case shaderc_geometry_shader: return vk::ShaderStageFlagBits::eGeometry;
            case shaderc_fragment_shader: return vk::ShaderStageFlagBits::eFragment;
            case shaderc_compute_shader: return vk::ShaderStageFlagBits::eCompute;
            default: VERIFY(false, ShaderKind); __builtin_unreachable();
        }
    }

    std::string_view SpvReflectResult2String(SpvReflectResult Result)
    {
        switch(Result)
        {
            case SPV_REFLECT_RESULT_SUCCESS: return "SPV_REFLECT_RESULT_SUCCESS";
            case SPV_REFLECT_RESULT_NOT_READY: return "SPV_REFLECT_RESULT_NOT_READY";
            case SPV_REFLECT_RESULT_ERROR_PARSE_FAILED: return "SPV_REFLECT_RESULT_ERROR_PARSE_FAILED";
            case SPV_REFLECT_RESULT_ERROR_ALLOC_FAILED: return "SPV_REFLECT_RESULT_ERROR_ALLOC_FAILED";
            case SPV_REFLECT_RESULT_ERROR_RANGE_EXCEEDED: return "SPV_REFLECT_RESULT_ERROR_RANGE_EXCEEDED";
            case SPV_REFLECT_RESULT_ERROR_NULL_POINTER: return "SPV_REFLECT_RESULT_ERROR_NULL_POINTER";
            case SPV_REFLECT_RESULT_ERROR_INTERNAL_ERROR: return "SPV_REFLECT_RESULT_ERROR_INTERNAL_ERROR";
            case SPV_REFLECT_RESULT_ERROR_COUNT_MISMATCH: return "SPV_REFLECT_RESULT_ERROR_COUNT_MISMATCH";
            case SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND: return "SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND";
            case SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_CODE_SIZE: return "SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_CODE_SIZE";
            case SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_MAGIC_NUMBER: return "SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_MAGIC_NUMBER";
            case SPV_REFLECT_RESULT_ERROR_SPIRV_UNEXPECTED_EOF: return "SPV_REFLECT_RESULT_ERROR_SPIRV_UNEXPECTED_EOF";
            case SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ID_REFERENCE: return "SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ID_REFERENCE";
            case SPV_REFLECT_RESULT_ERROR_SPIRV_SET_NUMBER_OVERFLOW: return "SPV_REFLECT_RESULT_ERROR_SPIRV_SET_NUMBER_OVERFLOW";
            case SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_STORAGE_CLASS: return "SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_STORAGE_CLASS";
            case SPV_REFLECT_RESULT_ERROR_SPIRV_RECURSION: return "SPV_REFLECT_RESULT_ERROR_SPIRV_RECURSION";
            case SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_INSTRUCTION: return "SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_INSTRUCTION";
            case SPV_REFLECT_RESULT_ERROR_SPIRV_UNEXPECTED_BLOCK_DATA: return "SPV_REFLECT_RESULT_ERROR_SPIRV_UNEXPECTED_BLOCK_DATA";
            case SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_BLOCK_MEMBER_REFERENCE: return "SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_BLOCK_MEMBER_REFERENCE";
            case SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ENTRY_POINT: return "SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ENTRY_POINT";
            case SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_EXECUTION_MODE: return "SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_EXECUTION_MODE";
            case SPV_REFLECT_RESULT_ERROR_SPIRV_MAX_RECURSIVE_EXCEEDED: return "SPV_REFLECT_RESULT_ERROR_SPIRV_MAX_RECURSIVE_EXCEEDED";
            default: return "unknown result";
        }
    }
}

SpvResultCheck& SpvResultCheck::operator=(SpvReflectResult Result_)
{
    VERIFY(Result_ == SPV_REFLECT_RESULT_SUCCESS, SpvReflectResult2String(Result_));
    Result = Result_;
    return *this;
}

std::future<VShader*> VShaderCache::CreateShader(const std::fpath& Path)
{
    auto[it, inserted] = Shaders.emplace(Path, VShader{});
    if(!inserted)
    {
        std::promise<VShader*> Promise{};
        Promise.set_value(&it->second);
        return Promise.get_future();
    }

    return TaskExecutor.async([=, this](){
        return CreateShader_Impl(&it->second, Path, false);
    });
}

bool VShaderCache::DestroyShader(const std::fpath& Path)
{
    auto Found = Shaders.find(Path);
    if(Found != Shaders.end())
    {
        LOG_INFO("destroying shader {}", Found->first);

        Context->Device.destroyShaderModule(Found->second.ShaderModule);
        spvReflectDestroyShaderModule(&Found->second.ReflectionModule);

        Shaders.unsafe_erase(Found);
        return true;
    }
    else
    {
        return false;
    }
}

VShader* VShaderCache::CreateShader_Impl(VShader* Shader, std::fpath Path, bool IsSpirv)
{
    LOG_INFO("creating shader {}", Path);

    std::vector<uint8_t> SourceData = ReadFileBinary(Path.c_str());

    shaderc::CompilationResult<uint32_t> CompileResult{};
    uint64_t CompiledSize;
    const uint32_t* CompiledData;

    if(IsSpirv)
    {
        CompiledSize = SourceData.size();
        CompiledData = reinterpret_cast<const uint32_t*>(SourceData.data());
    }
    else
    {
        LOG_INFO("compiling {}", Path);

        shaderc_shader_kind ShaderKind = ShaderName2ShaderKind(Path);

        shaderc::CompileOptions CompileOptions{};
        CompileOptions.SetSourceLanguage(shaderc_source_language_glsl);
        CompileOptions.SetTargetEnvironment(shaderc_target_env_vulkan, VK_API_VERSION_1_3);
        CompileOptions.SetTargetSpirv(shaderc_spirv_version_1_6);
        CompileOptions.SetOptimizationLevel(shaderc_optimization_level_performance);
        CompileOptions.SetPreserveBindings(true);

        shaderc_util::FileFinder FileFinder{};
        CompileOptions.SetIncluder(std::make_unique<glslc::FileIncluder>(&FileFinder));

#ifndef NDEBUG
        CompileOptions.SetGenerateDebugInfo();
#endif

        CompileResult = shaderc::Compiler().CompileGlslToSpv((char*)SourceData.data(), SourceData.size(), ShaderKind, Path.c_str(), "main", CompileOptions);
        VERIFY(CompileResult.GetCompilationStatus() == shaderc_compilation_status_success, CompileResult.GetErrorMessage());

        if(CompileResult.GetNumWarnings() > 0)
        {
            LOG_WARNING("{}", CompileResult.GetErrorMessage());
        }

        CompiledSize = std::distance(CompileResult.begin(), CompileResult.end()) * sizeof(uint32_t);
        CompiledData = CompileResult.begin();
    }

    LOG_INFO("building reflection data for {}", Path);

    spvResultCheck = spvReflectCreateShaderModule2(0, CompiledSize, CompiledData, &Shader->ReflectionModule);

    auto ModuleInfo = vk::ShaderModuleCreateInfo{}
            .setPCode(CompiledData)
            .setCodeSize(CompiledSize);

    vkResultCheck = Context->Device.createShaderModule(&ModuleInfo, nullptr, &Shader->ShaderModule);

    return Shader;
}

VShaderCache::VShaderCache(VContext* Context_)
{
    Context = Context_;
}

VShaderCache::~VShaderCache()
{
    TaskExecutor.wait_for_all();

    LOG_INFO("destroying shader cache");

    for(auto& Pair : Shaders)
    {
        LOG_INFO("destroying shader {}", Pair.first);

        Context->Device.destroyShaderModule(Pair.second.ShaderModule);
        spvReflectDestroyShaderModule(&Pair.second.ReflectionModule);
    }
}



