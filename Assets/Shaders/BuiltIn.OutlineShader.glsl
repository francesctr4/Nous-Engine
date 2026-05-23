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
    // Transform to clip space first.
    vec4 clipPos = globalUBO.projection * globalUBO.view
                 * pushConstant.model * vec4(inPosition, 1.0);

    if (pushConstant.outlineThickness > 0.0)
    {
        // Transform the welded smooth normal to view space.
        // Using view*model normal matrix so the normal is in view space, whose
        // XY plane maps directly to screen X/Y.
        mat3 normalMV = transpose(inverse(mat3(globalUBO.view * pushConstant.model)));
        vec3 viewNormal = normalize(normalMV * inSmoothNormal);

        // Project normal onto the screen plane (drop Z).
        // If the normal points almost directly at/away from the camera its XY
        // projection is near zero — use a small epsilon to avoid division by zero.
        vec2 screenNormal = viewNormal.xy;
        float len = length(screenNormal);
        if (len > 1e-4)
            screenNormal /= len;
        else
            screenNormal = vec2(0.0);

        // Offset in clip space (multiply by w so the offset is constant in NDC,
        // giving a screen-space-fixed pixel width independent of depth and mesh size).
        clipPos.xy += screenNormal * pushConstant.outlineThickness * clipPos.w;
    }

    gl_Position = clipPos;
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
