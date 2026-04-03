#pragma stage vertex
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;

// Data Transfer Object
layout(location = 0) out struct DataTransferObject
{
    vec3 outColor;
    vec2 texCoord;
} outDTO;

layout(set = 0, binding = 0) uniform globalUniformObject
{
    mat4 projection;
    mat4 view;
} globalUBO;

layout(push_constant) uniform pushConstantObject
{
// Only guaranteed a total of 128 bytes.
    mat4 model; // 64 bytes
} pushConstant;

void main()
{
    outDTO.outColor = inColor;
    outDTO.texCoord = inTexCoord;

    gl_Position = globalUBO.projection * globalUBO.view * pushConstant.model * vec4(inPosition, 1.0);
}

// ------------------------------------------------------------------------------------------------------

#pragma stage fragment
#version 450

// Data Transfer Object
layout(location = 0) in struct DataTransferObject
{
    vec3 outColor;
    vec2 texCoord;
} inDTO;

layout(location = 0) out vec4 fragColor;

layout(set = 1, binding = 0) uniform localUniformObject
{
    vec4 diffuseColor;
} localUBO;

// Samplers
layout(set = 1, binding = 1) uniform sampler2D diffuseSampler;
layout(set = 1, binding = 2) uniform sampler2D normalSampler;

void main()
{
    vec4 diffuse = localUBO.diffuseColor * texture(diffuseSampler, inDTO.texCoord);
    vec3 normalSample = texture(normalSampler, inDTO.texCoord).rgb;
    fragColor = diffuse * vec4(normalSample, 1.0);
}