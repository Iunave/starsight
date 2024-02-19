#version 460
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_shader_16bit_storage : require
#extension GL_EXT_shader_explicit_arithmetic_types : require

layout(set = 0, binding = 0) uniform sampler2D Textures[];

layout(location = 0) in vec3 vs_position;
layout(location = 1) in vec3 vs_normal;
layout(location = 2) in vec2 uv;
layout(location = 3) flat in uint32_t texIndex;

layout(location = 0) out vec4 color;

void main()
{
    if(texIndex == 0)
    {
        color = vec4(1.0, 1.0, 1.0, 1.0);
    }
    else
    {
        color = texture(Textures[texIndex], uv);

        if(color.a == 0)
        {
            discard;
        }
    }
}