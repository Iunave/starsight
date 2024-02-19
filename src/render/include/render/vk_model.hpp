#ifndef STARSIGHT_VK_MODEL_HPP
#define STARSIGHT_VK_MODEL_HPP

#include "vk_memory_allocator.hpp"
#include "vk_utility.hpp"
#include "core/ssovector.hpp"
#include "core/math.hpp"
#include "core/filesystem.hpp"
#include "core/resource.hpp"
#include "assimp/material.h"
#include "taskflow/taskflow.hpp"
#include "tbb/concurrent_unordered_map.h"

#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>

class VContext;

namespace Assimp
{
    class Importer;
}

class aiScene;
class aiMesh;
struct aiTexture;
struct aiNode;

struct NormalUV
{
    uint32_t Normal;
    uint32_t UV;
};

struct VStagingData
{
    vk::Buffer StagingBuffer = nullptr;
    vma::Allocation Allocation = nullptr;
    vk::CommandBuffer CommandBuffer = nullptr;
};

struct VTransferData
{
    std::atomic<vk::Semaphore> TransferFinish;
    std::optional<VStagingData> Staging;

    VTransferData()
        : TransferFinish(nullptr)
        , Staging()
    {
    }

    VTransferData(const VTransferData& Other)
        : TransferFinish(Other.TransferFinish.load(std::memory_order_relaxed))
        , Staging(Other.Staging)
    {
    }

    bool IsFinished() const;
    void WaitUntilFinished() const;
};

struct VMesh : public VTransferData, public SharedAsset
{
    glm::fvec4 SphereBounds; //w = radius, xyz = center
    uint32_t IndexCount = 0;
    uint32_t VertexCount = 0;
    BufferAllocationSlot IndexSlot{};
    BufferAllocationSlot PositionSlot{};
    BufferAllocationSlot NormalUVSlot{};
};

struct VTexture : public VTransferData, public SharedAsset
{
    vk::Extent2D Extent{};
    uint64_t MipMaps = 0;
    aiTextureType Type{};
    uint32_t DescriptorSlot = 0;
    vk::Image Image = nullptr;
    vk::ImageView ImageView = nullptr;
    vma::Allocation Allocation = nullptr;
    vma::AllocationInfo AllocationInfo{};
};

namespace scene
{
    struct NodeTransform
    {
        glm::vec3 Translation;
        glm::quat Rotation;
        glm::vec3 Scale;
    };

    struct TextureData
    {
        std::string Name;
        aiTextureType Type;
    };

    struct MeshData
    {
        std::string MeshName;
        std::vector<TextureData> Textures;
    };

    struct SceneNode
    {
        virtual ~SceneNode() = default;

        NodeTransform Transform;
        SceneNode* Parent;
        std::vector<std::unique_ptr<SceneNode>> Children;
    };

    struct MeshNode : public SceneNode
    {
        std::vector<MeshData> Meshes;
    };
}

class VModel : public SharedAsset
{
public:
    static VModel* LoadAsset(const std::fpath& Path);

public:

    enum ProgressState
    {
        InConstruction, //the node structure is currently being constructed
        InTransfer, //data is being uploaded to the GPU
        Ready //structure has been built and data is available on the GPU
    };

    std::unique_ptr<scene::SceneNode> RootNode;
    std::atomic<ProgressState> State;


    VModel()
        : RootNode(nullptr)
        , State(InConstruction)
    {
    }

    VModel(VModel&& Other)
        : RootNode(std::move(Other.RootNode))
        , State(Other.State.load(std::memory_order_relaxed))
    {
    }

    bool IsLoaded();

    void ForEachNode(const std::function<void(scene::SceneNode*)>& callback);
};

class VModelManager
{
public:
    tbb::concurrent_unordered_map<std::string, VTexture> Textures;
    tbb::concurrent_unordered_map<std::string, VMesh> Meshes;
    tbb::concurrent_unordered_map<std::fpath, VModel> Models;

    vk::Sampler TextureSampler = nullptr;

    tf::Executor TaskExecutor;
    VContext* Context;
public:

    VModelManager(VContext* Context_);
    ~VModelManager();

    VModel* LoadModel(const std::fpath& Path);
    void GarbageCollect(bool InDestruction = false);

private:

    void LoadModel_Impl(TAssetPtr<VModel> Asset);
    void RecurseNodes(std::unique_ptr<scene::SceneNode>& SceneNode, scene::SceneNode* ParentNode, aiNode* ImportNode, std::shared_ptr<Assimp::Importer> Importer, TAssetPtr<VModel> Asset);
    void ProcessMeshNode(scene::MeshNode* MeshNode, aiMesh* ImportMesh, uint64_t MeshIndex, std::shared_ptr<Assimp::Importer> Importer, TAssetPtr<VModel> Asset);

    void LoadMesh(VMesh* OutMesh, aiMesh* ImportMesh, std::shared_ptr<Assimp::Importer> Importer, TAssetPtr<VModel> Asset, std::string_view MeshName);

    void LoadFileTexture(VTexture* OutTexture, std::fpath Path, std::shared_ptr<Assimp::Importer> Importer, TAssetPtr<VModel> Asset);
    void LoadEmbeddedTexture(VTexture* OutTexture, const aiTexture* EmbeddedTexture, std::shared_ptr<Assimp::Importer> Importer, TAssetPtr<VModel> Asset);
    void LoadTexture(uint8_t* Pixels, uint64_t Width, uint64_t Height, VTexture* OutTexture, std::string Name);
};

#endif //STARSIGHT_VK_MODEL_HPP
