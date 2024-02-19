#include "vk_model.hpp"
#include "core/assertion.hpp"
#include "core/log.hpp"
#include "core/filesystem.hpp"
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/mesh.h"
#include "assimp/material.h"
#include "assimp/postprocess.h"
#include "image.hpp"
#include "vk_context.hpp"
#include "vk_render_target.hpp"
#include "core/utility_functions.hpp"

static glm::vec3 aiVec2glmVec(aiVector3D aiV)
{
    glm::vec3 glmV;
    glmV.x = aiV.x;
    glmV.y = aiV.y;
    glmV.z = aiV.z;
    return glmV;
}

static glm::quat aiQuat2glmQuat(aiQuaternion aiQ)
{
    glm::quat glmQ;
    glmQ.w = aiQ.w;
    glmQ.x = aiQ.x;
    glmQ.y = aiQ.y;
    glmQ.z = aiQ.z;
    return glmQ;
}

bool VTransferData::IsFinished() const
{
    vk::Semaphore Semaphore = TransferFinish.load(std::memory_order_relaxed);
    if(Semaphore == nullptr)
    {
        return false;
    }

    uint64_t Value;
    vkResultCheck = vkContext->Device.getSemaphoreCounterValue(Semaphore, &Value);

    return Value == 1;
}

void VTransferData::WaitUntilFinished() const
{
    vk::Semaphore Semaphore = TransferFinish.load(std::memory_order_relaxed);
    ASSERT(Semaphore != nullptr);

    uint64_t WaitValue = 1;

    auto WaitInfo = vk::SemaphoreWaitInfo{}
            .setSemaphores(Semaphore)
            .setValues(WaitValue);

    vkResultCheck = vkContext->Device.waitSemaphores(WaitInfo, vkutil::default_timeout);
}

bool VModel::IsLoaded()
{
    switch(State.load(std::memory_order_acquire))
    {
        case InConstruction:
        {
            return false;
        }
        case InTransfer:
        {
            bool NodesLoaded = true;
            ForEachNode([&NodesLoaded](scene::SceneNode* Node)
            {
                if(const auto* MeshNode = dynamic_cast<scene::MeshNode*>(Node))
                {
                    for(const auto& Mesh: MeshNode->Meshes)
                    {
                        NodesLoaded &= vkContext->ModelManager->Meshes.at(Mesh.MeshName).IsFinished();

                        for(const auto& Texture : Mesh.Textures)
                        {
                            NodesLoaded &= vkContext->ModelManager->Textures.at(Texture.Name).IsFinished();
                        }
                    }
                }
            });

            if(NodesLoaded)
            {
                State.store(Ready, std::memory_order_release);
                State.notify_all();
                return true;
            }
            else
            {
                return false;
            }
        }
        case Ready:
        {
            return true;
        }
    }
}

static void ForEachNode_Recurse(scene::SceneNode* Node, const std::function<void(scene::SceneNode*)>& Callback)
{
    Callback(Node);

    for(auto& Child : Node->Children)
    {
        ForEachNode_Recurse(Child.get(), Callback);
    }
}

void VModel::ForEachNode(const std::function<void(scene::SceneNode*)>& callback)
{
    ForEachNode_Recurse(RootNode.get(), callback);
}

VModel* VModel::LoadAsset(const std::fpath& Path)
{
    return vkContext->ModelManager->LoadModel(Path);
}

VModelManager::VModelManager(VContext* Context_)
{
    LOG_INFO("creating model manager");
    Context = Context_;

    auto SamplerInfo = vk::SamplerCreateInfo{}
    .setAddressModeU(vk::SamplerAddressMode::eMirroredRepeat)
    .setAddressModeV(vk::SamplerAddressMode::eMirroredRepeat)
    .setAddressModeW(vk::SamplerAddressMode::eMirroredRepeat)
    .setAnisotropyEnable(true)
    .setMaxAnisotropy(std::min(8.0f, Context->PhysicalDeviceProperties.properties.limits.maxSamplerAnisotropy))
    .setUnnormalizedCoordinates(false)
    .setCompareEnable(false)
    .setMipmapMode(vk::SamplerMipmapMode::eLinear)
    .setMinFilter(vk::Filter::eLinear)
    .setMagFilter(vk::Filter::eLinear)
    .setMinLod(0.f)
    .setMaxLod(64.f);

    vkResultCheck = Context->Device.createSampler(&SamplerInfo, nullptr, &TextureSampler);
    Context->NameObject(TextureSampler, "Model Manager Texture Sampler");
}

VModelManager::~VModelManager()
{
    TaskExecutor.wait_for_all();

    LOG_INFO("destroying model manager");

    GarbageCollect(true);
    
    VERIFY(Textures.size() == 0, ASSERTION::NONFATAL);
    VERIFY(Meshes.size() == 0, ASSERTION::NONFATAL);
    VERIFY(Models.size() == 0, ASSERTION::NONFATAL);

    Context->Device.destroySampler(TextureSampler);
    TextureSampler = nullptr;
}

VModel* VModelManager::LoadModel(const std::fpath& Path)
{
    auto[it, inserted] = Models.emplace(Path, VModel{});
    if(inserted)
    {
        TaskExecutor.silent_async([=, this](){
            LoadModel_Impl(TAssetPtr{it->first, &it->second});
        });
    }

    return &it->second;
}

void VModelManager::LoadModel_Impl(TAssetPtr<VModel> Asset)
{
    LOG_INFO("loading model - {}", Asset.GetPath());

    auto Importer = std::make_shared<Assimp::Importer>();
    Importer->SetPropertyInteger(AI_CONFIG_PP_RVC_FLAGS, aiComponent_COLORS | aiComponent_CAMERAS);
    Importer->SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_LINE | aiPrimitiveType_POINT);
    Importer->SetPropertyInteger(AI_CONFIG_PP_SLM_TRIANGLE_LIMIT, UINT16_MAX / 3u);
    Importer->SetPropertyInteger(AI_CONFIG_PP_SLM_VERTEX_LIMIT, INT32_MAX);
    Importer->SetPropertyInteger(AI_CONFIG_PP_ICL_PTCACHE_SIZE, 24); //Just a guess

    uint32_t PostprocessFlags =
            aiProcess_Triangulate
            | aiProcess_SortByPType
            | aiProcess_JoinIdenticalVertices
            | aiProcess_OptimizeMeshes
            | aiProcess_OptimizeGraph
            | aiProcess_ImproveCacheLocality
            | aiProcess_FlipUVs
            | aiProcess_GenNormals
            | aiProcess_GenBoundingBoxes
            | aiProcess_RemoveComponent
            | aiProcess_RemoveRedundantMaterials
            | aiProcess_FindInvalidData
            | aiProcess_GenUVCoords
            | aiProcess_TransformUVCoords
            | aiProcess_FindInstances
            | aiProcess_GlobalScale;
            //| aiProcess_SplitLargeMeshes;

#ifndef NDEBUG
    Importer->SetExtraVerbose(true);
    PostprocessFlags |= aiProcess_ValidateDataStructure;
#endif

    const aiScene* Scene = Importer->ReadFile(Asset.GetPath(), PostprocessFlags);
    VERIFY(Scene && Scene->mRootNode && !(Scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE), Importer->GetErrorString());

    VModel* Model = Asset.GetPtr();
    RecurseNodes(Model->RootNode, nullptr, Scene->mRootNode, Importer, Asset);

    Model->State.store(VModel::InTransfer, std::memory_order_relaxed);
}

void VModelManager::RecurseNodes(std::unique_ptr<scene::SceneNode>& SceneNode, scene::SceneNode* ParentNode, aiNode* ImportNode, std::shared_ptr<Assimp::Importer> Importer, TAssetPtr<VModel> Asset)
{
    if(ImportNode->mNumMeshes == 0)
    {
        SceneNode = std::make_unique<scene::SceneNode>();
    }
    else
    {
        SceneNode = std::make_unique<scene::MeshNode>();
    }

    SceneNode->Parent = ParentNode;

    aiVector3D Translation; aiQuaternion Rotation; aiVector3D Scale;
    ImportNode->mTransformation.Decompose(Scale, Rotation, Translation);

    SceneNode->Transform.Translation = aiVec2glmVec(Translation);
    SceneNode->Transform.Rotation = aiQuat2glmQuat(Rotation);
    SceneNode->Transform.Scale = aiVec2glmVec(Scale);

    for(uint64_t Mesh = 0; Mesh < ImportNode->mNumMeshes; ++Mesh)
    {
        uint64_t MeshIndex = ImportNode->mMeshes[Mesh];
        aiMesh* ImportMesh = Importer->GetScene()->mMeshes[MeshIndex];
        auto* MeshNode = dynamic_cast<scene::MeshNode*>(SceneNode.get());

        ProcessMeshNode(MeshNode, ImportMesh, MeshIndex, Importer, Asset);
    }

    for(uint64_t Child = 0; Child < ImportNode->mNumChildren; ++Child)
    {
        std::unique_ptr<scene::SceneNode>& SceneChildNode = SceneNode->Children.emplace_back();
        aiNode* ImportChildNode = ImportNode->mChildren[Child];

        RecurseNodes(SceneChildNode, SceneNode.get(), ImportChildNode, Importer, Asset);
    }
}

void VModelManager::ProcessMeshNode(scene::MeshNode* MeshNode, aiMesh* ImportMesh, uint64_t MeshIndex, std::shared_ptr<Assimp::Importer> Importer, TAssetPtr<VModel> Asset)
{
    std::string SceneName{Importer->GetScene()->mName.data, Importer->GetScene()->mName.length};
    std::string MeshName{ImportMesh->mName.data, ImportMesh->mName.length};

    if(MeshName.empty())
    {
        MeshName = "empty_name";
    }

    auto& MeshData = MeshNode->Meshes.emplace_back();
    MeshData.MeshName = fmt::format("{}.{}.[{}]", SceneName, MeshName, MeshIndex);

    {
        auto[it, inserted] = Meshes.emplace(MeshData.MeshName, VMesh{});
        it->second.AddReference();

        if(inserted)
        {
            TaskExecutor.silent_async([=, this](){
                LoadMesh(&it->second, ImportMesh, Importer, Asset, it->first);
            });
        }
    }

    aiMaterial* Material = Importer->GetScene()->mMaterials[ImportMesh->mMaterialIndex];

    auto ProcessTexture = [&](aiTextureType TextureType)
    {
        for(uint64_t idx = 0; idx < Material->GetTextureCount(TextureType); ++idx)
        {
            auto& Texture = MeshData.Textures.emplace_back();
            Texture.Type = TextureType;

            aiString AiTextureName;
            VERIFY(Material->GetTexture(TextureType, idx, &AiTextureName) == AI_SUCCESS);

            const aiTexture* EmbeddedTexture = Importer->GetScene()->GetEmbeddedTexture(AiTextureName.C_Str());

            if(EmbeddedTexture)
            {
                Texture.Name = fmt::format("{}.{}", SceneName, AiTextureName.C_Str());
            }
            else
            {
                Texture.Name = Asset.GetPath().parent_path().append(AiTextureName.C_Str());
            }

            {
                auto[it, inserted] = Textures.emplace(Texture.Name, VTexture{});
                it->second.AddReference();

                if(inserted)
                {
                    it->second.Type = TextureType;

                    if(EmbeddedTexture)
                    {
                        TaskExecutor.silent_async([=, this](){
                            LoadEmbeddedTexture(&it->second, EmbeddedTexture, Importer, Asset);
                        });
                    }
                    else
                    {
                        TaskExecutor.silent_async([=, this](){
                            LoadFileTexture(&it->second, Texture.Name, Importer, Asset);
                        });
                    }
                }
            }
        }
    };

    ProcessTexture(aiTextureType_BASE_COLOR);
/*
    for(std::underlying_type_t<aiTextureType> TextureType = 0; TextureType <= AI_TEXTURE_TYPE_MAX; ++TextureType)
    {
        ProcessTexture(static_cast<aiTextureType>(TextureType));
    }
    */
}

void VModelManager::LoadMesh(VMesh* OutMesh, aiMesh* ImportMesh, std::shared_ptr<Assimp::Importer> Importer, TAssetPtr<VModel> Asset, std::string_view MeshName)
{
    LOG_INFO("loading mesh - {}", MeshName);

    OutMesh->IndexCount = ImportMesh->mNumFaces * 3u;
    OutMesh->VertexCount = ImportMesh->mNumVertices;

    const uint32_t IndexBufferSize = OutMesh->IndexCount * sizeof(uint32_t);
    const uint32_t PositionBufferSize = OutMesh->VertexCount * sizeof(glm::fvec3);
    const uint32_t NormalUVBufferSize = OutMesh->VertexCount * sizeof(NormalUV);

    glm::vec3 BoundsMin = aiVec2glmVec(ImportMesh->mAABB.mMin);
    glm::vec3 BoundsMax = aiVec2glmVec(ImportMesh->mAABB.mMax);
    glm::vec3 BoundsCenter = (BoundsMin + BoundsMax) / 2.0f;
    float BoundsRadius = glm::distance(BoundsMin, BoundsMax) / 2.0f;
    OutMesh->SphereBounds = glm::vec4{BoundsCenter, BoundsRadius};

    VAllocatedBuffer StagingBuffer = Context->AllocateBuffer(
            IndexBufferSize + PositionBufferSize + NormalUVBufferSize,
            vk::BufferUsageFlagBits::eTransferSrc,
            vma::AllocationCreateFlagBits::eMapped | vma::AllocationCreateFlagBits::eHostAccessRandom | vma::AllocationCreateFlagBits::eStrategyFirstFit,
            vma::MemoryUsage::eAutoPreferHost,
            fmt::format("{} [index, normal, uv] staging buffer", MeshName)
    );

    auto* Indices = reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(StagingBuffer.MappedData) + 0);
    auto* Positions = reinterpret_cast<glm::fvec3*>(static_cast<uint8_t*>(StagingBuffer.MappedData) + IndexBufferSize);
    auto* NormalsUVs = reinterpret_cast<NormalUV*>(static_cast<uint8_t*>(StagingBuffer.MappedData) + IndexBufferSize + PositionBufferSize);

    for(uint64_t face = 0; face < ImportMesh->mNumFaces; ++face)
    {
        ASSERT(ImportMesh->mFaces[face].mNumIndices == 3);
        for(uint64_t index = 0; index < ImportMesh->mFaces[face].mNumIndices; ++index)
        {
            ASSERT(ImportMesh->mFaces[face].mIndices[index] <= UINT32_MAX);
            Indices[(face * 3) + index] = ImportMesh->mFaces[face].mIndices[index];
        }
    }

    for(uint64_t vertex = 0; vertex < ImportMesh->mNumVertices; ++vertex)
    {
        Positions[vertex] = aiVec2glmVec(ImportMesh->mVertices[vertex]);

        if(ImportMesh->HasNormals())
        {
            glm::vec3 Normal = aiVec2glmVec(ImportMesh->mNormals[vertex]);
            glm::vec2 OctNormal = math::oct_encode(glm::normalize(Normal));
            NormalsUVs[vertex].Normal = glm::packSnorm2x16(OctNormal);
        }
        else
        {
            NormalsUVs[vertex].Normal = 0;
        }

        if(ImportMesh->HasTextureCoords(0))
        {
            glm::vec3 UVW = aiVec2glmVec(ImportMesh->mTextureCoords[0][vertex]);
            NormalsUVs[vertex].UV = glm::packUnorm2x16(UVW.xy);
        }
        else
        {
            NormalsUVs[vertex].UV = 0;
        }
    }

    OutMesh->IndexSlot = vkContext->GrabIndexBufferMemory(IndexBufferSize, 4u);
    OutMesh->PositionSlot = vkContext->GrabVertexBufferMemory(PositionBufferSize, 4u);
    OutMesh->NormalUVSlot = vkContext->GrabVertexBufferMemory(NormalUVBufferSize, 8u);

    auto TimelineInfo = vk::SemaphoreTypeCreateInfo{}
            .setSemaphoreType(vk::SemaphoreType::eTimeline)
            .setInitialValue(0);

    auto SemaphoreInfo = vk::SemaphoreCreateInfo{}
            .setPNext(&TimelineInfo);

    vk::Semaphore Semaphore;
    vkResultCheck = Context->Device.createSemaphore(&SemaphoreInfo, nullptr, &Semaphore);
    Context->NameObject(Semaphore, fmt::format("TransferFinish {}", MeshName));

    OutMesh->TransferFinish.store(Semaphore, std::memory_order_relaxed);

    vk::CommandBuffer CommandBuffer = Context->RequestTransferCommandBuffer(std::string{MeshName});

    OutMesh->Staging.emplace();
    OutMesh->Staging->Allocation = StagingBuffer.Allocation;
    OutMesh->Staging->StagingBuffer = StagingBuffer.Buffer;
    OutMesh->Staging->CommandBuffer = CommandBuffer;

    auto IndexCopyRegion = vk::BufferCopy2{}
            .setDstOffset(OutMesh->IndexSlot.Offset)
            .setSrcOffset(0)
            .setSize(IndexBufferSize);

    auto PositionCopyRegion = vk::BufferCopy2{}
            .setDstOffset(OutMesh->PositionSlot.Offset)
            .setSrcOffset(IndexBufferSize)
            .setSize(PositionBufferSize);

    auto NormalUVCopyRegion = vk::BufferCopy2{}
            .setDstOffset(OutMesh->NormalUVSlot.Offset)
            .setSrcOffset(IndexBufferSize + PositionBufferSize)
            .setSize(NormalUVBufferSize);

    auto IndexCopyBufferInfo = vk::CopyBufferInfo2{}
            .setDstBuffer(vkContext->GlobalIndexBuffer.Buffer)
            .setSrcBuffer(StagingBuffer.Buffer)
            .setRegions(IndexCopyRegion);

    auto PositionCopyBufferInfo = vk::CopyBufferInfo2{}
            .setDstBuffer(vkContext->GlobalVertexBuffer.Buffer)
            .setSrcBuffer(StagingBuffer.Buffer)
            .setRegions(PositionCopyRegion);

    auto NormalUVCopyBufferInfo = vk::CopyBufferInfo2{}
            .setDstBuffer(vkContext->GlobalVertexBuffer.Buffer)
            .setSrcBuffer(StagingBuffer.Buffer)
            .setRegions(NormalUVCopyRegion);

    CommandBuffer.copyBuffer2(IndexCopyBufferInfo);
    CommandBuffer.copyBuffer2(PositionCopyBufferInfo);
    CommandBuffer.copyBuffer2(NormalUVCopyBufferInfo);

    auto CommandBufferSubmitInfo = vk::CommandBufferSubmitInfo{}
            .setCommandBuffer(CommandBuffer)
            .setDeviceMask(1);

    auto SignalInfo = vk::SemaphoreSubmitInfo{}
            .setSemaphore(Semaphore)
            .setValue(1);

    auto SubmitInfo = vk::SubmitInfo2{}
            .setCommandBufferInfos(CommandBufferSubmitInfo)
            .setSignalSemaphoreInfos(SignalInfo);

    Context->SubmitTransferCommands(CommandBuffer, SubmitInfo);

    LOG_INFO("finished loading mesh - {}", MeshName);
}

static constexpr void PreCalculateTextureSizeAndMips(uint64_t width, uint64_t height, uint64_t pixel_size, uint64_t* OutSize, uint64_t* OutMipMaps)
{
    *OutSize = 0;
    *OutMipMaps = 0;

    while(width > 0 && height > 0)
    {
        *OutSize += width * height * pixel_size;
        *OutMipMaps += 1;

        width /= 2;
        height /= 2;
    }
}

static vk::Format aiTextureType2vkFormat(aiTextureType Type)
{
    switch(Type)
    {
        case aiTextureType_DIFFUSE: return vk::Format::eR8G8B8A8Srgb;
        case aiTextureType_SPECULAR: return vk::Format::eR8G8B8A8Srgb;
        case aiTextureType_AMBIENT: return vk::Format::eUndefined;
        case aiTextureType_EMISSIVE: return vk::Format::eUndefined;
        case aiTextureType_HEIGHT: return vk::Format::eUndefined;
        case aiTextureType_NORMALS: return vk::Format::eR8G8B8A8Snorm;
        case aiTextureType_SHININESS: return vk::Format::eUndefined;
        case aiTextureType_OPACITY: return vk::Format::eUndefined;
        case aiTextureType_DISPLACEMENT: return vk::Format::eUndefined;
        case aiTextureType_LIGHTMAP: return vk::Format::eR32Sfloat;
        case aiTextureType_REFLECTION: return vk::Format::eUndefined;
        case aiTextureType_BASE_COLOR: return vk::Format::eR8G8B8A8Srgb;
        case aiTextureType_NORMAL_CAMERA: return vk::Format::eUndefined;
        case aiTextureType_EMISSION_COLOR: return vk::Format::eUndefined;
        case aiTextureType_METALNESS: return vk::Format::eUndefined;
        case aiTextureType_DIFFUSE_ROUGHNESS: return vk::Format::eUndefined;
        case aiTextureType_AMBIENT_OCCLUSION: return vk::Format::eUndefined;
        case aiTextureType_SHEEN: return vk::Format::eUndefined;
        case aiTextureType_CLEARCOAT: return vk::Format::eUndefined;
        case aiTextureType_TRANSMISSION: return vk::Format::eUndefined;
        case aiTextureType_UNKNOWN: return vk::Format::eUndefined;
        default: return vk::Format::eUndefined;
    }
}

static void MipMapImage(const glm::vec<4, uint8_t>* src, uint64_t src_width, uint64_t src_height, glm::vec<4, uint8_t>* dst, uint64_t dst_width, uint64_t dst_height)
{
    for(uint64_t dst_y = 0; dst_y < dst_height; ++dst_y)
    {
        for(uint64_t dst_x = 0; dst_x < dst_width; ++dst_x)
        {
            uint64_t src_x = dst_x << 1;
            uint64_t src_y = dst_y << 1;
            ASSERT(src_x + 1 < src_width && src_y + 1 < src_height);

            glm::fvec4 px0 = src[(src_x + 0) + ((src_y + 0) * src_width)];
            glm::fvec4 px1 = src[(src_x + 0) + ((src_y + 1) * src_width)];
            glm::fvec4 px2 = src[(src_x + 1) + ((src_y + 0) * src_width)];
            glm::fvec4 px3 = src[(src_x + 1) + ((src_y + 1) * src_width)];

            dst[dst_x + dst_y * dst_width] = glm::round((px0 + px1 + px2 + px3) / 4.0f);
        }
    }
}

void VModelManager::LoadTexture(uint8_t* Pixels, uint64_t Width, uint64_t Height, VTexture* OutTexture, std::string Name)
{
    vk::Format ImageFormat = aiTextureType2vkFormat(OutTexture->Type);
    VERIFY((ImageFormat != vk::Format::eUndefined), OutTexture->Type);

    uint64_t TextureSize;
    uint64_t MipMaps;
    uint64_t PixelSize = 4;
    PreCalculateTextureSizeAndMips(Width, Height, PixelSize, &TextureSize, &MipMaps);

    OutTexture->Extent = vk::Extent2D{static_cast<uint32_t>(Width), static_cast<uint32_t>(Height)};
    OutTexture->MipMaps = MipMaps;

    VAllocatedBuffer StagingBuffer = Context->AllocateBuffer(
            TextureSize,
            vk::BufferUsageFlagBits::eTransferSrc, vma::AllocationCreateFlagBits::eMapped | vma::AllocationCreateFlagBits::eHostAccessRandom | vma::AllocationCreateFlagBits::eStrategyFirstFit,
            vma::MemoryUsage::eAutoPreferHost);

    memcpy(StagingBuffer.MappedData, Pixels, Width * Height * PixelSize);

    uint8_t* Source = nullptr;
    uint8_t* Dest = static_cast<uint8_t*>(StagingBuffer.MappedData);

    for(uint64_t MipMapLevel = 0; MipMapLevel < MipMaps; ++MipMapLevel)
    {
        uint64_t SrcWidth = Width >> MipMapLevel;
        uint64_t SrcHeight = Height >> MipMapLevel;
        uint64_t DstWidth = SrcWidth >> 1;
        uint64_t DstHeight = SrcHeight >> 1;

        Source = Dest;
        Dest += SrcWidth * SrcHeight * PixelSize;

        MipMapImage(reinterpret_cast<glm::vec<4, uint8_t>*>(Source), SrcWidth, SrcHeight, reinterpret_cast<glm::vec<4, uint8_t>*>(Dest), DstWidth, DstHeight);
    }

    auto TextureImageInfo = vk::ImageCreateInfo{}
            .setUsage(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst)
            .setArrayLayers(1)
            .setExtent(vk::Extent3D{static_cast<uint32_t>(Width), static_cast<uint32_t>(Height), 1})
            .setMipLevels(MipMaps)
            .setFormat(ImageFormat)
            .setImageType(vk::ImageType::e2D)
            .setTiling(vk::ImageTiling::eOptimal)
            .setSamples(vk::SampleCountFlagBits::e1)
            .setInitialLayout(vk::ImageLayout::eUndefined);

    auto TextureAllocationInfo = vma::AllocationCreateInfo{}
            .setFlags(vma::AllocationCreateFlagBits::eStrategyBestFit)
            .setUsage(vma::MemoryUsage::eAutoPreferDevice);

    vkResultCheck = Context->Allocator.createImage(&TextureImageInfo, &TextureAllocationInfo, &OutTexture->Image, &OutTexture->Allocation, &OutTexture->AllocationInfo);
    Context->NameObject(OutTexture->Image, fmt::format("{} image", Name));

    auto ImageViewInfo = vk::ImageViewCreateInfo{}
            .setImage(OutTexture->Image)
            .setFormat(ImageFormat)
            .setViewType(vk::ImageViewType::e2D)
            .setComponents(vk::ComponentMapping{})
            .setSubresourceRange(vk::ImageSubresourceRange{
                    vk::ImageAspectFlagBits::eColor,
                    0,
                    static_cast<uint32_t>(MipMaps),
                    0,
                    1
            });

    OutTexture->ImageView = Context->Device.createImageView(ImageViewInfo);
    Context->NameObject(OutTexture->ImageView, fmt::format("{} image view", Name));

    auto TimelineInfo = vk::SemaphoreTypeCreateInfo{}
            .setSemaphoreType(vk::SemaphoreType::eTimeline)
            .setInitialValue(0);

    auto SemaphoreInfo = vk::SemaphoreCreateInfo{}
            .setPNext(&TimelineInfo);

    vk::Semaphore Semaphore;
    vkResultCheck = Context->Device.createSemaphore(&SemaphoreInfo, nullptr, &Semaphore);
    Context->NameObject(Semaphore, fmt::format("TransferFinish {}", Name));

    OutTexture->TransferFinish.store(Semaphore, std::memory_order_relaxed);

    //protected by mutex from here
    vk::CommandBuffer CommandBuffer = Context->RequestTransferCommandBuffer(Name);

    OutTexture->Staging.emplace();
    OutTexture->Staging->Allocation = StagingBuffer.Allocation;
    OutTexture->Staging->StagingBuffer = StagingBuffer.Buffer;
    OutTexture->Staging->CommandBuffer = CommandBuffer;

    std::vector<vk::BufferImageCopy2> CopyRegions(MipMaps);
    uint32_t BufferOffset = 0;
    for(uint32_t MipMapLevel = 0; MipMapLevel < MipMaps; ++MipMapLevel)
    {
        uint32_t CopyWidth = Width >> MipMapLevel;
        uint32_t CopyHeight = Height >> MipMapLevel;

        CopyRegions[MipMapLevel]
                .setImageExtent(vk::Extent3D{CopyWidth, CopyHeight, 1})
                .setImageOffset(vk::Offset3D{0, 0, 0})
                .setBufferRowLength(0)
                .setBufferImageHeight(0)
                .setBufferOffset(BufferOffset)
                .setImageSubresource(vk::ImageSubresourceLayers{
                        vk::ImageAspectFlagBits::eColor,
                        MipMapLevel,
                        0,
                        1
                });

        BufferOffset += CopyWidth * CopyHeight * PixelSize;
    }

    auto CopyBuffer2ImageInfo = vk::CopyBufferToImageInfo2{}
            .setDstImage(OutTexture->Image)
            .setDstImageLayout(vk::ImageLayout::eTransferDstOptimal)
            .setSrcBuffer(StagingBuffer.Buffer)
            .setRegions(CopyRegions);

    auto Undefined2TransferDst = vk::ImageMemoryBarrier2{}
            .setImage(OutTexture->Image)
            .setOldLayout(vk::ImageLayout::eUndefined)
            .setNewLayout(vk::ImageLayout::eTransferDstOptimal)
            .setSrcStageMask(vk::PipelineStageFlagBits2::eNone)
            .setSrcAccessMask(vk::AccessFlagBits2::eNone)
            .setDstStageMask(vk::PipelineStageFlagBits2::eTransfer)
            .setDstAccessMask(vk::AccessFlagBits2::eTransferWrite)
            .setSubresourceRange(vk::ImageSubresourceRange{
                    vk::ImageAspectFlagBits::eColor,
                    0,
                    static_cast<uint32_t>(MipMaps),
                    0,
                    1
            });

    auto TransferDst2ReadOnly = vk::ImageMemoryBarrier2{}
            .setImage(OutTexture->Image)
            .setOldLayout(vk::ImageLayout::eTransferDstOptimal)
            .setNewLayout(vk::ImageLayout::eReadOnlyOptimal)
            .setSrcStageMask(vk::PipelineStageFlagBits2::eTransfer)
            .setSrcAccessMask(vk::AccessFlagBits2::eTransferWrite)
            .setDstStageMask(vk::PipelineStageFlagBits2::eNone)
            .setDstAccessMask(vk::AccessFlagBits2::eNone)
            .setSubresourceRange(vk::ImageSubresourceRange{
                    vk::ImageAspectFlagBits::eColor,
                    0,
                    static_cast<uint32_t>(MipMaps),
                    0,
                    1
            });

    auto Undefined2TransferDst_Dependency = vk::DependencyInfo{}.setImageMemoryBarriers(Undefined2TransferDst);
    auto TransferDst2ReadOnly_Dependency = vk::DependencyInfo{}.setImageMemoryBarriers(TransferDst2ReadOnly);

    CommandBuffer.pipelineBarrier2(Undefined2TransferDst_Dependency);
    CommandBuffer.copyBufferToImage2(CopyBuffer2ImageInfo);
    CommandBuffer.pipelineBarrier2(TransferDst2ReadOnly_Dependency);

    auto CommandBufferSubmitInfo = vk::CommandBufferSubmitInfo{}
            .setCommandBuffer(CommandBuffer)
            .setDeviceMask(1);

    auto SignalInfo = vk::SemaphoreSubmitInfo{}
            .setSemaphore(OutTexture->TransferFinish)
            .setValue(1);

    auto SubmitInfo = vk::SubmitInfo2{}
            .setCommandBufferInfos(CommandBufferSubmitInfo)
            .setSignalSemaphoreInfos(SignalInfo);

    Context->SubmitTransferCommands(CommandBuffer, SubmitInfo);

    OutTexture->DescriptorSlot = Context->GrabDescriptorSlot(vk::DescriptorType::eCombinedImageSampler);

    auto DescriptorImageInfo = vk::DescriptorImageInfo{}
    .setImageLayout(vk::ImageLayout::eReadOnlyOptimal)
    .setImageView(OutTexture->ImageView)
    .setSampler(TextureSampler);

    auto WriteDescriptorSet = vk::WriteDescriptorSet{}
            .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
            .setDescriptorCount(1)
            .setDstSet(Context->ShaderResourceSet)
            .setDstBinding(0)
            .setDstArrayElement(OutTexture->DescriptorSlot)
            .setImageInfo(DescriptorImageInfo);

    Context->Device.updateDescriptorSets(WriteDescriptorSet, {});
}

void VModelManager::LoadFileTexture(VTexture* OutTexture, std::fpath Path, std::shared_ptr<Assimp::Importer> Importer, TAssetPtr<VModel> Asset)
{
    LOG_INFO("loading file texture - {}", Path);

    FILE* file = fopen(Path.c_str(), "r");
    VERIFY(file != nullptr, strerror(errno));

    int32_t Width; int32_t Height; int32_t Channels;
    uint8_t* Pixels = stbi_load_from_file(file, &Width, &Height, &Channels, 4);
    VERIFY(Pixels != nullptr, stbi_failure_reason());

    fclose(file);

    LoadTexture(Pixels, Width, Height, OutTexture, Path);
    SafeFree(Pixels)

    LOG_INFO("finished loading file texture - {}", Path);
}

void VModelManager::LoadEmbeddedTexture(VTexture* OutTexture, const aiTexture* EmbeddedTexture, std::shared_ptr<Assimp::Importer> Importer, TAssetPtr<VModel> Asset)
{
    LOG_INFO("loading embedded texture - {}", EmbeddedTexture->mFilename.C_Str());

    if(EmbeddedTexture->mHeight == 0)
    {
        int32_t Width; int32_t Height; int32_t Channels;
        uint8_t* Pixels = stbi_load_from_memory(reinterpret_cast<uint8_t*>(EmbeddedTexture->pcData), EmbeddedTexture->mWidth, &Width, &Height, &Channels, 4);
        VERIFY(Pixels != nullptr, stbi_failure_reason());

        LoadTexture(Pixels, Width, Height, OutTexture, EmbeddedTexture->mFilename.C_Str());
        SafeFree(Pixels);
    }
    else
    {
        LoadTexture(reinterpret_cast<uint8_t*>(EmbeddedTexture->pcData), EmbeddedTexture->mWidth, EmbeddedTexture->mHeight, OutTexture, EmbeddedTexture->mFilename.C_Str());
    }

    LOG_INFO("finished loading embedded texture - {}", EmbeddedTexture->mFilename.C_Str());
}

void VModelManager::GarbageCollect(bool InDestruction)
{
    if(TaskExecutor.num_topologies() != 0)
    {
        return;
    }

    bool bModelIsLoaded;
    bool bFreeModel;
    std::function<void(scene::SceneNode* Node)> CheckSceneNode = [this, &bModelIsLoaded, &bFreeModel, InDestruction](scene::SceneNode* Node)
    {
        if(auto* MeshNode = dynamic_cast<scene::MeshNode*>(Node))
        {
            for(const auto& MeshRef : MeshNode->Meshes)
            {
                {
                    auto MeshIt = Meshes.find(MeshRef.MeshName);
                    VMesh& Mesh = MeshIt->second;

                    if(bFreeModel)
                    {
                        if(Mesh.RemoveReference() == 1)
                        {
                            auto Destruction = [Copy = Mesh]()
                            {
                                vkContext->Device.destroySemaphore(Copy.TransferFinish);

                                vkContext->FreeIndexBufferMemory(Copy.IndexSlot);
                                vkContext->FreeVertexBufferMemory(Copy.PositionSlot);
                                vkContext->FreeVertexBufferMemory(Copy.NormalUVSlot);
                            };

                            if(InDestruction)
                            {
                                Destruction();
                            }
                            else
                            {
                                vkContext->DeferredDestructionQueue.enqueue(Destruction);
                            }

                            LOG_DEBUG("GC,d mesh {}", MeshIt->first);
                            Meshes.unsafe_erase(MeshIt);
                        }
                    }
                    else
                    {
                        if(Mesh.IsFinished())
                        {
                            if(Mesh.Staging.has_value())
                            {
                                LOG_DEBUG("GC,d staging buffer {}", MeshIt->first);
                                Context->Allocator.destroyBuffer(Mesh.Staging->StagingBuffer, Mesh.Staging->Allocation);
                                Context->FreeTransferCommandBuffer(Mesh.Staging->CommandBuffer);
                                Mesh.Staging.reset();
                            }
                        }
                        else
                        {
                            bModelIsLoaded = false;
                        }
                    }
                }

                for(const auto& TextureRef : MeshRef.Textures)
                {
                    auto TextureIt = Textures.find(TextureRef.Name);
                    VTexture& Texture = TextureIt->second;

                    if(bFreeModel)
                    {
                        if(Texture.RemoveReference() == 1)
                        {
                            auto Destruction = [Copy = Texture]()
                            {
                                vkContext->FreeDescriptorSlot(vk::DescriptorType::eCombinedImageSampler, Copy.DescriptorSlot);
                                vkContext->Device.destroySemaphore(Copy.TransferFinish);
                                vkContext->Device.destroyImageView(Copy.ImageView);
                                vkContext->Allocator.destroyImage(Copy.Image, Copy.Allocation);
                            };

                            if(InDestruction)
                            {
                                Destruction();
                            }
                            else
                            {
                                vkContext->DeferredDestructionQueue.enqueue(Destruction);
                            }

                            LOG_DEBUG("GC,d texture {}", TextureIt->first);
                            Textures.unsafe_erase(TextureIt);
                        }
                    }
                    else
                    {
                        if(Texture.IsFinished())
                        {
                            if(Texture.Staging.has_value())
                            {
                                LOG_DEBUG("GC,d staging buffer {}", TextureIt->first);
                                Context->Allocator.destroyBuffer(Texture.Staging->StagingBuffer, Texture.Staging->Allocation);
                                Context->FreeTransferCommandBuffer(Texture.Staging->CommandBuffer);
                                Texture.Staging.reset();
                            }
                        }
                        else
                        {
                            bModelIsLoaded = false;
                        }
                    }
                }
            }
        }
    };

    auto it = Models.begin();
    while(it != Models.end())
    {
        bModelIsLoaded = true;
        bFreeModel = false;
        it->second.ForEachNode(CheckSceneNode);

        if(bModelIsLoaded && it->second.GetRefCount() == 0)
        {
            bModelIsLoaded = true;
            bFreeModel = true;
            it->second.ForEachNode(CheckSceneNode);

            LOG_DEBUG("GC,d model {}", it->first);
            it = Models.unsafe_erase(it);
        }
        else
        {
            ++it;
        }
    }
}




