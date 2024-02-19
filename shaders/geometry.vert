#version 460
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_shader_16bit_storage : require
#extension GL_EXT_shader_explicit_arithmetic_types : require
#include "utility.glsl"

struct NormalUV
{
    uint32_t normal;
    uint32_t uv;
};

struct Transform
{
    vec3 translation;
    vec3 translation_err;
    vec4 rotation;
    vec3 scale;
};

layout(std430, buffer_reference, buffer_reference_align = 256) readonly buffer CameraBuffer
{
    mat4x4 view;
    mat4x4 projection;
    mat4x4 viewProjection;
    vec3 location;
    vec3 location_err;
    vec4 frustum;
    float near;
    float far;
    float pad3[2];
};

layout(scalar, buffer_reference, buffer_reference_align = 4) readonly buffer TransformBuffer
{
    Transform transforms[];
};

layout(scalar, buffer_reference, buffer_reference_align = 4) readonly buffer PositionBuffer
{
    vec3 positions[];
};

layout(scalar, buffer_reference, buffer_reference_align = 8) readonly buffer NormalUVBuffer
{
    NormalUV normalUVs[];
};

struct Mesh
{
    uint32_t indexCount;
    uint32_t vertexCount;
    uint32_t indexBufferOffset;
    uint32_t positionBufferOffset;
    uint32_t normalUVBufferOffset;
    uint32_t baseColorIndex;
};

layout(scalar, buffer_reference, buffer_reference_align = 4) readonly buffer MeshBuffer
{
    Mesh meshes[];
};

layout(scalar, push_constant) uniform PC
{
    CameraBuffer pCamera;
    MeshBuffer pMeshes;
    TransformBuffer pTransform;
    uint64_t pVertexBuffer;
};

layout(location = 0) out vec3 vs_position;
layout(location = 1) out vec3 vs_normal;
layout(location = 2) out vec2 uv;
layout(location = 3) out uint32_t texIndex;

void main()
{
    Mesh mesh = pMeshes.meshes[gl_BaseInstance];

    Transform transform = pTransform.transforms[gl_BaseInstance];
    transform.rotation = transform.rotation.yzwx;

    vec3 vertexPos = PositionBuffer(pVertexBuffer + uint64_t(mesh.positionBufferOffset)).positions[gl_VertexIndex];
    uint32_t vertexNormal = NormalUVBuffer(pVertexBuffer + uint64_t(mesh.normalUVBufferOffset)).normalUVs[gl_VertexIndex].normal;
    uint32_t vertexUV = NormalUVBuffer(pVertexBuffer + uint64_t(mesh.normalUVBufferOffset)).normalUVs[gl_VertexIndex].uv;

    vec3 delta_pos = (transform.translation - pCamera.location) - (transform.translation_err - pCamera.location_err);
    vs_position = quatRotateVec(transform.rotation, vertexPos) * transform.scale + delta_pos;

    vec3 ws_normal = quatRotateVec(transform.rotation, octDecode(unpackSnorm2x16(vertexNormal)));
    //vs_normal = normalize((pCamera.view * vec4(ws_normal, 0.0)).xyz);
    vs_normal = ws_normal;

    uv = unpackUnorm2x16(vertexUV);
    texIndex = uint32_t(mesh.baseColorIndex);

    gl_Position = pCamera.viewProjection * vec4(vs_position, 1.0);
}