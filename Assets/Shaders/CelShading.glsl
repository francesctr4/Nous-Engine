#pragma stage vertex
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;

layout(location = 0) out struct DataTransferObject
{
    vec3 worldNormal;
    vec2 texCoord;
} outDTO;

layout(set = 0, binding = 0) uniform globalUniformObject
{
    mat4 projection;
    mat4 view;
} globalUBO;

layout(push_constant) uniform pushConstantObject
{
    mat4 model;
} pushConstant;

void main()
{
    // Approximate world-space normal (correct for uniform scale).
    outDTO.worldNormal = mat3(pushConstant.model) * inNormal;
    outDTO.texCoord    = inTexCoord;

    gl_Position = globalUBO.projection * globalUBO.view * pushConstant.model * vec4(inPosition, 1.0);
}

// ------------------------------------------------------------------------------------------------------

#pragma stage fragment
#version 450

layout(location = 0) in struct DataTransferObject
{
    vec3 worldNormal;
    vec2 texCoord;
} inDTO;

layout(location = 0) out vec4 fragColor;

layout(set = 1, binding = 0) uniform localUniformObject
{
    vec4 diffuseColor;
} localUBO;

layout(set = 1, binding = 1) uniform sampler2D diffuseSampler;

void main()
{
    // Fixed directional light (world space).
    const vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));

    vec3  n     = normalize(inDTO.worldNormal);
    float NdotL = max(dot(n, lightDir), 0.0);

    // Quantize diffuse into 3 hard bands: shadow / mid / highlight.
    float cel = floor(NdotL * 3.0) / 3.0;

    vec4 texColor = texture(diffuseSampler, inDTO.texCoord);
    fragColor = localUBO.diffuseColor * texColor * (0.15 + cel * 0.85);
}
