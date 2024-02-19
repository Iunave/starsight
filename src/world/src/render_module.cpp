#include "render_module.hpp"
#include "core/log.hpp"
#include "render/vk_context.hpp"
#include "render/vk_render_target.hpp"
#include "window/window.hpp"
#include "input_module.hpp"
#include "core/ssovector.hpp"

RenderModule::RenderModule(flecs::world& world)
{
    Self = this;
    Renderer = dynamic_cast<VStarSightRenderer*>(vkContext);

    world.module<RenderModule>("Render");

    //world.component<CameraComponent>("Camera");
    world.component<ModelComponent>("Model");
    world.component<MeshComponent>("Mesh");
    world.component<TransformComponent>("Transform");

    world.system<ModelComponent>("Load Models")
            .kind(flecs::OnLoad)
            .write<TransformComponent>()
            .write<ModelComponent>()
            .write<DirtyTag>()
            .each(LoadModels);

    world.system<TransformComponent, DirtyTag>("Update Dirty Graph Transforms")
            .kind(flecs::PreUpdate)
            .read<TransformComponent>()
            .write<TransformComponent>()
            .read<DirtyTag>()
            .write<DirtyTag>()
            .each(UpdateGraphTransforms);

    world.system("Prepare Device Buffers")
            .kind(flecs::PreUpdate)
            .read<ModelComponent>()
            .iter(PrepareDeviceBuffers);

    world.system<TransformComponent, MeshComponent>("Upload Mesh Data")
            .kind(flecs::OnUpdate)
            .read<TransformComponent>()
            .read<ModelComponent>()
            .multi_threaded(true)
            .each(UploadMeshData);

    world.system<CameraComponent>("Upload Camera Data")
            .kind(flecs::OnUpdate)
            .each(UploadCameraData);

    world.system("ModelManager GC")
            .kind(flecs::PostUpdate)
            .interval(10)
            .iter(GarbageCollect);

    world.system("Flush Device Data")
            .kind(flecs::PreStore)
            .iter(FlushDeviceData);

    world.system("Draw")
            .kind(flecs::OnStore)
            .iter(Draw);
}

RenderModule::~RenderModule()
{
    Self = nullptr;
}

void RenderModule::GarbageCollect(flecs::iter&)
{
    vkContext->ModelManager->GarbageCollect();
}

void RenderModule::BuildNodeEntities(flecs::iter& it, flecs::entity Parent, scene::SceneNode* Node, bool RootNode)
{
    flecs::entity NodeEntity;
    if(RootNode) //treat the entity with the model component as the root node
    {
        NodeEntity = Parent;

        TransformComponent RootTransform{};
        if(auto* ModelTransform = Parent.get<TransformComponent>())
        {
            RootTransform = *ModelTransform;
        }

        RootTransform.location += Node->Transform.Translation;
        RootTransform.rotation = RootTransform.rotation * Node->Transform.Rotation;
        RootTransform.scale *= Node->Transform.Scale;

        Parent.set<TransformComponent>(RootTransform);
        Parent.add<DirtyTag>();
    }
    else
    {
        NodeEntity = it.world().entity()
                .add(flecs::ChildOf, Parent)
                .set<TransformComponent>({
                    .location = Node->Transform.Translation,
                    .rotation = Node->Transform.Rotation,
                    .scale = Node->Transform.Scale
                });
    }

    if(auto* MeshNode = dynamic_cast<scene::MeshNode*>(Node))
    {
        for(scene::MeshData& Mesh : MeshNode->Meshes)
        {
            it.world().entity()
                    .add(flecs::ChildOf, NodeEntity)
                    .set<TransformComponent>({})
                    .set<MeshComponent>(Mesh);
        }
    }

    for(const std::unique_ptr<scene::SceneNode>& ChildNode : Node->Children)
    {
        BuildNodeEntities(it, NodeEntity, ChildNode.get(), false);
    }
}

void RenderModule::LoadModels(flecs::iter& it, size_t index, ModelComponent& Model)
{
    if(!Model.isBuilt && !Model.Asset.IsLoaded()) [[unlikely]]
    {
        Model.Asset.Load();
        return;
    }

    if(!Model.isBuilt) [[unlikely]]
    {
        Model.isBuilt = true;

        flecs::entity Parent = it.entity(index);
        BuildNodeEntities(it, Parent, Model.Asset->RootNode.get(), true);
    }
}

void RenderModule::UpdateGraphTransforms(flecs::iter& it, size_t index, const TransformComponent&, DirtyTag)
{
    flecs::entity Entity = it.entity(index);
    Entity.remove<DirtyTag>();

    ssovector<flecs::entity, 32> FinalChildren{};
    std::function<void(flecs::entity)> AddChildren = [&FinalChildren, &AddChildren](flecs::entity Child) -> void
    {
        bool HasChildren = false;
        Child.children([&HasChildren, &AddChildren](flecs::entity Child)
        {
            HasChildren = true;
            AddChildren(Child);
        });

        if(!HasChildren)
        {
            FinalChildren.emplace_back(Child);
        }
    };

    Entity.children([&AddChildren](flecs::entity Child){
        AddChildren(Child);
    });

    for(flecs::entity Child : FinalChildren)
    {
        auto* ChildTransform = Child.get_mut<TransformComponent>();
        *ChildTransform = TransformComponent{}; //reset the transform

        flecs::entity Parent = Child.parent();
        while(Parent.is_valid())
        {
            if(const auto* ParentTransform = Parent.get<TransformComponent>())
            {
                ChildTransform->location += ParentTransform->location;
                ChildTransform->rotation = ParentTransform->rotation * ChildTransform->rotation;
                ChildTransform->scale *= ParentTransform->scale;
            }

            Parent = Parent.parent();
        }
    }
}

void RenderModule::PrepareDeviceBuffers(flecs::iter& it)
{
    Self->MeshCount.store(0, std::memory_order_relaxed);

    uint64_t TotalMeshCount = it.world().count<MeshComponent>();

    uint64_t DrawCommandsCount = (Renderer->DrawIndirectCommandsBuffer.Size - sizeof(VShaderDrawIndirectCount)) / sizeof(vk::DrawIndexedIndirectCommand);
    uint64_t MeshTransformCount = Renderer->ActiveFrame->MeshTransforms.Size / sizeof(VShaderTransform);
    uint64_t MeshInfoCount = Renderer->ActiveFrame->MeshInfos.Size / sizeof(VShaderMeshInfo);
    uint64_t MeshBoundsCount = Renderer->ActiveFrame->MeshBounds.Size / sizeof(glm::fvec4);

    auto BufferSize = [TotalMeshCount](uint64_t ElementSize){
        return math::PadSize2Alignment(DEVICE_MESH_ALLOCATION_STEP + (TotalMeshCount * ElementSize), uint64_t(DEVICE_MESH_ALLOCATION_STEP));
    };

    if(TotalMeshCount > DrawCommandsCount || TotalMeshCount + DEVICE_MESH_ALLOCATION_STEP * 2 < DrawCommandsCount) [[unlikely]]
    {
        vkContext->ReallocateBuffer(&Renderer->DrawIndirectCommandsBuffer, sizeof(VShaderDrawIndirectCount) + BufferSize(sizeof(vk::DrawIndexedIndirectCommand)));
    }

    if(TotalMeshCount > MeshTransformCount || TotalMeshCount + DEVICE_MESH_ALLOCATION_STEP * 2 < MeshTransformCount) [[unlikely]]
    {
        vkContext->ReallocateBuffer(&Renderer->ActiveFrame->MeshTransforms, BufferSize(sizeof(VShaderTransform)));
    }

    if(TotalMeshCount > MeshInfoCount || TotalMeshCount + DEVICE_MESH_ALLOCATION_STEP * 2 < MeshInfoCount) [[unlikely]]
    {
        vkContext->ReallocateBuffer(&Renderer->ActiveFrame->MeshInfos, BufferSize(sizeof(VShaderMeshInfo)));
    }

    if(TotalMeshCount > MeshBoundsCount || TotalMeshCount + DEVICE_MESH_ALLOCATION_STEP * 2 < MeshBoundsCount) [[unlikely]]
    {
        vkContext->ReallocateBuffer(&Renderer->ActiveFrame->MeshBounds, BufferSize(sizeof(glm::fvec4)));
    }
}

void RenderModule::UploadMeshData(const TransformComponent& Transform, const MeshComponent& Mesh)
{
    const uint32_t thisIndex = Self->MeshCount.fetch_add(1, std::memory_order_relaxed);

    void* pMeshTransform = static_cast<VShaderTransform*>(Renderer->ActiveFrame->MeshTransforms.MappedData) + thisIndex;
    void* pMeshInfo = static_cast<VShaderMeshInfo*>(Renderer->ActiveFrame->MeshInfos.MappedData) + thisIndex;
    void* pMeshBounds = static_cast<glm::fvec4*>(Renderer->ActiveFrame->MeshBounds.MappedData) + thisIndex;

    //https://godotengine.org/article/emulating-double-precision-gpu-render-large-worlds/
    VShaderTransform ShaderTransform{};
    ShaderTransform.Translation = glm::fvec3(Transform.location);
    ShaderTransform.Translation_Err = glm::fvec3(Transform.location - glm::dvec3(ShaderTransform.Translation));
    ShaderTransform.Rotation = glm::fquat(Transform.rotation);
    ShaderTransform.Scale = glm::fvec3(Transform.scale);

    VShaderMeshInfo ShaderMeshInfo{};
    ShaderMeshInfo.indexCount = Mesh.indexCount;
    ShaderMeshInfo.vertexCount = Mesh.vertexCount;
    ShaderMeshInfo.indexBufferOffset = Mesh.indexBufferOffset;
    ShaderMeshInfo.positionBufferOffset = Mesh.positionBufferOffset;
    ShaderMeshInfo.normalUVBufferOffset = Mesh.normalUVBufferOffset;
    ShaderMeshInfo.baseColorIndex = Mesh.baseColorIndex;

    glm::fvec4 ShaderMeshBounds = Mesh.SphereBounds;

    memcpy(pMeshTransform, &ShaderTransform, sizeof(ShaderTransform));
    memcpy(pMeshInfo, &ShaderMeshInfo, sizeof(ShaderMeshInfo));
    memcpy(pMeshBounds, &ShaderMeshBounds, sizeof(ShaderMeshBounds));
}

void RenderModule::UploadCameraData(const CameraComponent& Camera)
{
    VShaderCameraData Data{};
    Data.View = Camera.MakeView();
    Data.Projection = Camera.MakeProjection(InputModule::Self->FramebufferSize.x, InputModule::Self->FramebufferSize.y);
    Data.ViewProjection = Data.Projection * Data.View;
    Data.Location = glm::fvec3{Camera.Location};
    Data.LocationErr = glm::fvec3{glm::dvec3{Data.Location} - Camera.Location};
    Data.Near = Camera.Near;
    Data.Far = Camera.Far;

    //https://github.com/zeux/niagara/blob/master/src/niagara.cpp#L841
    glm::dmat4x4 ProjectionT = glm::transpose(Data.Projection);
    glm::dvec4 frustumX = math::NormalizePlane(ProjectionT[3] + ProjectionT[0]); // x + w < 0
    glm::dvec4 frustumY = math::NormalizePlane(ProjectionT[3] + ProjectionT[1]); // y + w < 0

    Data.Frustum[0] = frustumX.x;
    Data.Frustum[1] = frustumX.z;
    Data.Frustum[2] = frustumY.y;
    Data.Frustum[3] = frustumY.z;

    uint64_t BufferOffset = Renderer->ActiveFrameIndex() * sizeof(VShaderCameraData);
    void* ShaderCamera = static_cast<uint8_t*>(Renderer->CameraBuffer.MappedData) + BufferOffset;

    memcpy(ShaderCamera, &Data, sizeof(VShaderCameraData));
}

void RenderModule::FlushDeviceData(flecs::iter&)
{
    uint32_t UploadedMeshesCount = Self->MeshCount.load(std::memory_order_relaxed);

    std::array Allocations{
            Renderer->CameraBuffer.Allocation,
            Renderer->ActiveFrame->MeshTransforms.Allocation,
            Renderer->ActiveFrame->MeshInfos.Allocation,
            Renderer->ActiveFrame->MeshBounds.Allocation,
    };

    std::array Offsets{
            vk::DeviceSize{Renderer->ActiveFrameIndex() * sizeof(VShaderCameraData)},
            vk::DeviceSize{0},
            vk::DeviceSize{0},
            vk::DeviceSize{0}
    };

    std::array Sizes{
            vk::DeviceSize{sizeof(VShaderCameraData)},
            vk::DeviceSize{sizeof(VShaderTransform) * UploadedMeshesCount},
            vk::DeviceSize{sizeof(VShaderMeshInfo) * UploadedMeshesCount},
            vk::DeviceSize{sizeof(glm::fvec4) * UploadedMeshesCount}
    };

    vkContext->Allocator.flushAllocations(Allocations, Offsets, Sizes);
}

void RenderModule::Draw(flecs::iter&)
{
    Renderer->DrawDeferred(Self->MeshCount.load(std::memory_order_relaxed));
}

