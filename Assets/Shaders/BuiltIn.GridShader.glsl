#pragma stage vertex
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 2) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

layout(set = 0, binding = 0) uniform GlobalUBO
{
    mat4 projection;
    mat4 view;
} globalUBO;

layout(push_constant) uniform PushConstants
{
    mat4 model;
} pc;

void main()
{
    gl_Position = globalUBO.projection * globalUBO.view * pc.model * vec4(inPosition, 1.0);
    fragColor = inColor;
}

// ------------------------------------------------------------------------------------------------------

#pragma stage fragment
#version 450

layout(location = 0) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

void main()
{
    outColor = vec4(fragColor, 1.0);
}
