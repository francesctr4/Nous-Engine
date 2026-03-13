#pragma stage vertex
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 4) in vec3 inSmoothNormal; // position-welded smooth normal, computed at import time

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
    float outlineThickness; // 4 bytes
} pushConstant;

void main()
{
    vec3 worldPos = vec3(pushConstant.model * vec4(inPosition, 1.0));

    // Use the welded smooth normal (not the per-face/UV-split render normal) so
    // that all vertex copies at the same position extrude identically, preventing
    // gaps and spikes at UV seams and hard edges.
    mat3 normalMatrix = transpose(inverse(mat3(pushConstant.model)));
    vec3 worldSmoothNormal = normalize(normalMatrix * inSmoothNormal);

    // Scale thickness proportionally to the model's world-space size so the
    // outline width stays consistent regardless of the mesh's scale transform.
    float scaleX = length(vec3(pushConstant.model[0]));
    float scaleY = length(vec3(pushConstant.model[1]));
    float scaleZ = length(vec3(pushConstant.model[2]));
    float modelScale = (scaleX + scaleY + scaleZ) / 3.0;

    worldPos += worldSmoothNormal * pushConstant.outlineThickness * modelScale;

    gl_Position = globalUBO.projection * globalUBO.view * vec4(worldPos, 1.0);
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
