#include "mesh_component.hpp"
#include "render/vk_context.hpp"

MeshComponent::MeshComponent(const scene::MeshData& MeshData)
{
    const VMesh& Mesh = vkContext->ModelManager->Meshes.at(MeshData.MeshName);

    SphereBounds = Mesh.SphereBounds;

    indexCount = Mesh.IndexCount;
    vertexCount = Mesh.VertexCount;
    indexBufferOffset = Mesh.IndexSlot.Offset;
    positionBufferOffset = Mesh.PositionSlot.Offset;
    normalUVBufferOffset = Mesh.NormalUVSlot.Offset;

    baseColorIndex = 0;
    for(const auto& TextureRef : MeshData.Textures)
    {
        if(TextureRef.Type == aiTextureType_BASE_COLOR || TextureRef.Type == aiTextureType_DIFFUSE)
        {
            const VTexture& Texture = vkContext->ModelManager->Textures.at(TextureRef.Name);
            baseColorIndex = Texture.DescriptorSlot;
            break;
        }
    }
}