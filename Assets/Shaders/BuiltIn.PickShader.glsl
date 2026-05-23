#pragma stage vertex
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;

layout(set = 0, binding = 0) uniform globalUniformObject
{
    mat4 projection;
    mat4 view;
} globalUBO;

layout(push_constant) uniform pushConstantObject
{
// Only guaranteed a total of 128 bytes.
    mat4 model;       // 64 bytes
    uint objectID;    //  4 bytes
} pushConstant;

layout(location = 0) flat out uint outObjectID;

void main()
{
    gl_Position = globalUBO.projection * globalUBO.view * pushConstant.model * vec4(inPosition, 1.0);
    outObjectID = pushConstant.objectID;
}

// ------------------------------------------------------------------------------------------------------

#pragma stage fragment
#version 450

layout(location = 0) flat in uint inObjectID;
layout(location = 0) out vec4 fragColor;

void main()
{
    // Encode the 32-bit object ID into RGBA8 channels (one byte per channel).
    // Blending is disabled on the pick pipeline, so all 4 channels are preserved.
    // Decoded on CPU as: id = R | (G << 8) | (B << 16) | (A << 24)
    uint id = inObjectID;
    fragColor = vec4(
        float(id & 0xFFu) / 255.0,
        float((id >> 8u) & 0xFFu) / 255.0,
        float((id >> 16u) & 0xFFu) / 255.0,
        float((id >> 24u) & 0xFFu) / 255.0
    );
}
