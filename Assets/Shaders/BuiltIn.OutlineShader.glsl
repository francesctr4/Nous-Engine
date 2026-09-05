#pragma stage vertex
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 4) in vec3 inSmoothNormal; // position-welded smooth normal, computed at import time
layout(location = 7) in uvec4 inBoneIDs;
layout(location = 8) in vec4  inBoneWeights;

layout(set = 0, binding = 0) uniform globalUniformObject
{
    mat4 projection;
    mat4 view;
    vec4 outlineColor;
} globalUBO;

// Its own region of the shared palette buffer: this pass iterates the selected
// objects in natural order, so it cannot share the scene pass's sorted bases.
layout(set = 0, binding = 3) readonly buffer BonePalette
{
    mat4 bones[];
} bonePalette;

const uint NO_SKIN = 0xFFFFFFFFu;

layout(push_constant) uniform pushConstantObject
{
// Only guaranteed a total of 128 bytes.
    mat4  model;            // 64 bytes
    float outlineThickness; //  4 bytes
    uint  paletteBase;      //  4 bytes
} pushConstant;

void main()
{
    vec3 position     = inPosition;
    vec3 smoothNormal = inSmoothNormal;

    // Two guards, covering different failures: the sentinel for a rigged mesh whose
    // animator has not bound yet, the weight test for an unweighted vertex inside a
    // skinned mesh.
    if (pushConstant.paletteBase != NO_SKIN && dot(inBoneWeights, vec4(1.0)) > 0.0)
    {
        uint base = pushConstant.paletteBase;
        mat4 skin = inBoneWeights.x * bonePalette.bones[base + inBoneIDs.x]
                  + inBoneWeights.y * bonePalette.bones[base + inBoneIDs.y]
                  + inBoneWeights.z * bonePalette.bones[base + inBoneIDs.z]
                  + inBoneWeights.w * bonePalette.bones[base + inBoneIDs.w];

        position = (skin * vec4(position, 1.0)).xyz;

        // The NORMAL must be skinned too, not just the position. The extrusion below
        // offsets along this normal, so leaving it in bind pose gives a shell that
        // leans the wrong way on a deformed character.
        smoothNormal = normalize(mat3(skin) * smoothNormal);
    }

    // Transform to clip space first.
    vec4 clipPos = globalUBO.projection * globalUBO.view
                 * pushConstant.model * vec4(position, 1.0);

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
