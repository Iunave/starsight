#ifndef STARSIGHT_VK_RENDER_TARGET_HPP
#define STARSIGHT_VK_RENDER_TARGET_HPP

#include "core/math.hpp"
#include "vk_utility.hpp"
#include "concurrentqueue.h"
#include "vk_context.hpp"
#include <vulkan/vulkan.hpp>
#include <span>
#include <functional>

#ifndef FRAMES_IN_FLIGHT
#define FRAMES_IN_FLIGHT 2
#endif

#ifndef DEVICE_MESH_ALLOCATION_STEP
#define DEVICE_MESH_ALLOCATION_STEP 1024
#endif

struct GLFWwindow;

struct VShaderTransform
{
    glm::fvec3 Translation;
    glm::fvec3 Translation_Err;
    glm::fquat Rotation;
    glm::fvec3 Scale;
};

struct VShaderMeshInfo
{
    uint32_t indexCount;
    uint32_t vertexCount;
    uint32_t indexBufferOffset;
    uint32_t positionBufferOffset;
    uint32_t normalUVBufferOffset;
    uint32_t baseColorIndex;
};

struct VShaderBuildDrawCommandsPC
{
    vk::DeviceAddress pCamera;
    vk::DeviceAddress pDrawIndirectCount;
    vk::DeviceAddress pMeshBounds;
    vk::DeviceAddress pMeshes;
    vk::DeviceAddress pTransforms;
    uint32_t meshCount;
};

struct VShaderForwardDrawPC
{
    vk::DeviceAddress pCamera;
    vk::DeviceAddress pMesh;
    vk::DeviceAddress pTransform;
    vk::DeviceAddress pVertexBuffer;
};

struct VShaderDrawIndirectCount
{
    uint32_t Count;
};

struct VGlobalLightPC
{
    glm::fvec3 lightDirection;
    glm::fvec3 lightColor;
    float lightStrength;
};

struct alignas(UNIFORM_BUFFER_ALIGNMENT) VShaderCameraData
{
    glm::fmat4x4 View;
    glm::fmat4x4 Projection;
    glm::fmat4x4 ViewProjection;
    glm::fvec3 Location;
    float pad0;
    glm::fvec3 LocationErr;
    float pad1;
    float Frustum[4];
    float Near;
    float Far;
    float pad3[2];
};

struct VFrame
{
    std::vector<std::function<void()>> OnFrameBegin{};

    vk::CommandBuffer CommandBuffer = nullptr;
    vk::Fence InFlight = nullptr;
    vk::Semaphore ImageAvailable = nullptr;
    vk::Semaphore DrawFinished = nullptr;

    VAllocatedBuffer MeshBounds{};
    VAllocatedBuffer MeshInfos{};
    VAllocatedBuffer MeshTransforms{};
};

class VStarSightRenderer : public VContext
{
public:
    GLFWwindow* Window = nullptr;

    vk::SurfaceKHR Surface = nullptr;
    vk::SurfaceFormatKHR SurfaceFormat{};
    vk::PresentModeKHR PresentMode{};

    vk::Extent2D ImageExtent{};
    uint32_t MinImageCount = 0;
    vk::SharingMode SharingMode{};

    vk::SwapchainKHR SwapChain = nullptr;
    std::vector<vk::Image> Images{};
    std::vector<vk::ImageView> ImageViews{};

    std::array<VFrame, FRAMES_IN_FLIGHT> Frames{};
    VFrame* ActiveFrame = nullptr;

    VAllocatedBuffer CameraBuffer{};
    VAllocatedBuffer DrawIndirectCommandsBuffer{};

    vk::PipelineLayout BuildDrawCommandsLayout = nullptr;
    vk::Pipeline BuildDrawCommandsPipeline = nullptr;

    vk::PipelineLayout ForwardPipelineLayout = nullptr;
    vk::Pipeline ForwardPipeline = nullptr;

    vk::PipelineLayout GeometryPipelineLayout = nullptr;
    vk::Pipeline GeometryPipeline = nullptr;

    vk::PipelineLayout GlobalLightPipelineLayout = nullptr;
    vk::Pipeline GlobalLightPipeline = nullptr;

    struct
    {
        vk::Format PositionFormat{};
        vk::Format NormalFormat{};
        vk::Format ColorFormat{};
        vk::Format DepthFormat{};

        VAllocatedImage Position{};
        VAllocatedImage Normal{};
        VAllocatedImage Color{};
        VAllocatedImage Depth{};
    } GBuffer;

public:

    VStarSightRenderer(GLFWwindow* Window_);
    virtual ~VStarSightRenderer();

    void WaitForFrames();
    void Draw(uint32_t MeshCount);
    void DrawDeferred(uint32_t MeshCount);
    //void DrawDeferred(std::span<ecs::RenderSystem::RenderInfo> RenderInfos);

    std::string_view GetWindowName() const;
    std::pair<uint32_t, uint32_t> GetWindowExtent();

    uint64_t ActiveFrameIndex() const;

private:
    friend class VContext;

    /*
     * initialization
     */
    void CreateSurface();
    void CreateSwapChain(vk::SwapchainKHR OldSwapChain = nullptr);
    void CreateSwapChainViews();
    void CreateFrames();
    void CreateDrawCommandsPipeline();
    void CreateForwardPipeline();
    void CreateGeometryPipeline();
    void CreateGlobalLightPipeline();
    void CreateCameraBuffer();
    void CreateIndirectCommandsBuffer();
    void CreateGBuffer();
    void DestroyGBuffer();

    /*
     * forward rendering
     */
    uint32_t AcquireSwapChainImage();
    void BeginSwapChainRender(uint32_t SwapChainImage);
    void DrawShader(uint32_t MeshCount);
    void EndSwapChainRender(uint32_t SwapChainImage);
    void SubmitCommands();
    bool PresentImage(uint32_t SwapChainImage);
    void RecreateSwapChain();
    void AdvanceActiveFrame();

    /*
     * deferred rendering
     */
    void BeginGeometryPass();
    //void RenderGBuffer(std::span<ecs::RenderSystem::RenderInfo> RenderInfos);
    void EndGeometryPass(uint32_t SwapChainImage);
    void SubmitGBufferCommands();
};

#endif //STARSIGHT_VK_RENDER_TARGET_HPP
