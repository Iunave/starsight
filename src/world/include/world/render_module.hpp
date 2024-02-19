#ifndef STARSIGHT_RENDER_MODULE_HPP
#define STARSIGHT_RENDER_MODULE_HPP

#include "transform_component.hpp"
#include "mesh_component.hpp"
#include "model_component.hpp"
#include "camera_component.hpp"
#include "flecs.h"

class VStarSightRenderer;

struct DirtyTag{};

class RenderModule
{
private:
    static inline constinit RenderModule* Self = nullptr;

    static void GarbageCollect(flecs::iter&);
    static void BuildNodeEntities(flecs::iter& it, flecs::entity Parent, scene::SceneNode* Node, bool RootNode);
    static void UpdateGraphTransforms(flecs::iter& it, size_t index, const TransformComponent&, DirtyTag);
    static void LoadModels(flecs::iter& it, size_t index, ModelComponent& Model);
    static void PrepareDeviceBuffers(flecs::iter& it);
    static void UploadMeshData(const TransformComponent& Transform, const MeshComponent& Mesh);
    static void UploadCameraData(const CameraComponent& Camera);
    static void FlushDeviceData(flecs::iter&);
    static void Draw(flecs::iter&);

public:
    std::atomic_uint32_t MeshCount = 0;
    static inline constinit VStarSightRenderer* Renderer = nullptr;

public:
    explicit RenderModule(flecs::world& world);

    RenderModule(RenderModule&& Other)
        : MeshCount(Other.MeshCount.load(std::memory_order_relaxed))
    {
    }

    RenderModule& operator=(RenderModule&& Other)
    {
        MeshCount = Other.MeshCount.load(std::memory_order_relaxed);
        return *this;
    }

    ~RenderModule();
};


#endif //STARSIGHT_RENDER_MODULE_HPP
