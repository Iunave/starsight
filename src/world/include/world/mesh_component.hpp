#ifndef STARSIGHT_MESH_COMPONENT_HPP
#define STARSIGHT_MESH_COMPONENT_HPP

#include "core/math.hpp"
#include "render/vk_model.hpp"

class MeshComponent
{
public:
    glm::fvec4 SphereBounds{};

    uint32_t indexCount = -1;
    uint32_t vertexCount = -1;
    uint32_t indexBufferOffset = -1;
    uint32_t positionBufferOffset = -1;
    uint32_t normalUVBufferOffset = -1;
    uint32_t baseColorIndex = -1;

    MeshComponent() = default;
    MeshComponent(const scene::MeshData& MeshData);
};

#endif //STARSIGHT_MESH_COMPONENT_HPP
