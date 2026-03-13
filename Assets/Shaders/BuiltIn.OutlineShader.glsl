#pragma stage vertex
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(set = 0, binding = 0) uniform globalUniformObject
{
    mat4 projection;
    mat4 view;
    vec4 outlineColor;
} globalUBO;

layout(push_constant) uniform pushConstantObject
{
// Only guaranteed a total of 128 bytes.
    mat4 model; // 64 bytes
} pushConstant;

void main()
{
    gl_Position = globalUBO.projection * globalUBO.view * pushConstant.model * vec4(inPosition, 1.0);
}

// ------------------------------------------------------------------------------------------------------

#pragma stage fragment
#version 450

layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 0) uniform globalUniformObject
{
    mat4 projection;
    mat4 view;
    vec4 outlineColor;
} globalUBO;

void main()
{
    fragColor = globalUBO.outlineColor;
}
