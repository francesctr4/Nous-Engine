#pragma stage vertex
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;
layout(location = 7) in uvec4 inBoneIDs;
layout(location = 8) in vec4  inBoneWeights;

layout(set = 0, binding = 0) uniform globalUniformObject
{
    mat4 projection;
    mat4 view;
} globalUBO;

// Same buffer the material shader reads; PickObjectAt packs its own region because it
// iterates geometries in natural order rather than GroupGeometries' sorted order.
layout(set = 0, binding = 3) readonly buffer BonePalette
{
    mat4 bones[];
} bonePalette;

const uint NO_SKIN = 0xFFFFFFFFu;

layout(push_constant) uniform pushConstantObject
{
// Only guaranteed a total of 128 bytes.
    mat4 model;       // 64 bytes
    uint objectID;    //  4 bytes
    uint paletteBase; //  4 bytes
} pushConstant;

layout(location = 0) flat out uint outObjectID;

void main()
{
    vec4 position = vec4(inPosition, 1.0);

    // Two guards, covering different failures: the sentinel for a rigged mesh whose
    // animator has not bound yet (non-zero weights, no uploaded palette), the weight
    // test for an unweighted vertex inside a skinned mesh (which would otherwise
    // accumulate a zero matrix and collapse to the origin).
    if (pushConstant.paletteBase != NO_SKIN && dot(inBoneWeights, vec4(1.0)) > 0.0)
    {
        uint base = pushConstant.paletteBase;
        mat4 skin = inBoneWeights.x * bonePalette.bones[base + inBoneIDs.x]
                  + inBoneWeights.y * bonePalette.bones[base + inBoneIDs.y]
                  + inBoneWeights.z * bonePalette.bones[base + inBoneIDs.z]
                  + inBoneWeights.w * bonePalette.bones[base + inBoneIDs.w];
        position = skin * position;
    }

    gl_Position = globalUBO.projection * globalUBO.view * pushConstant.model * position;
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
