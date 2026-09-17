#pragma stage vertex
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;
layout(location = 7) in uvec4 inBoneIDs;
layout(location = 8) in vec4  inBoneWeights;

struct DirectionalLight { vec4 direction; vec4 color; };
struct PointLight        { vec4 position; vec4 color; };
struct SpotLight         { vec4 position; vec4 direction; vec4 color; vec4 angles; };

layout(set = 0, binding = 0) uniform GlobalUBO
{
    mat4             projection;
    mat4             view;
    vec4             viewPosition;
    vec4             ambientColor;
    DirectionalLight dirLight;
    ivec4            lightCountAndPad;
    PointLight       pointLights[16];
    vec4             time;  // x=totalTime, y=sin(t), z=cos(t), w=deltaTime
    ivec4            spotLightCountAndPad;
    SpotLight        spotLights[8];
} globalUBO;

layout(set = 0, binding = 1) readonly buffer InstanceData
{
    mat4 models[];
} instanceData;

// Optional capability: a shader that omits bindings 2 and 3 is valid, it just renders
// skinned meshes in bind pose (and the backend warns once).
layout(set = 0, binding = 2) readonly buffer PaletteBases
{
    uint bases[];
} paletteBases;

layout(set = 0, binding = 3) readonly buffer BonePalette
{
    mat4 bones[];
} bonePalette;

const uint NO_SKIN = 0xFFFFFFFFu;

// Blends this vertex's bone matrices into `skin`, in MODEL space. Returns false -- and
// leaves `skin` untouched -- when the vertex must not be skinned, so a static mesh pays
// one branch and no matrix maths.
//
// The two guards cover DIFFERENT failures and both are load-bearing:
//
// The SENTINEL covers a rigged mesh whose animator has not bound yet: its weights are
// non-zero, so a weights-only test would index into a palette that was never uploaded.
//
// The WEIGHT TEST covers an unweighted vertex inside a skinned mesh, which would
// otherwise accumulate a zero matrix and collapse to the origin.
//
// These are the same two rules AnimationSystem's SkinVertices implements, so the GPU
// path and the tested CPU reference agree by construction. Keep every copy of this
// function identical: there is no #include in this shader pipeline, so it is duplicated
// per shader rather than shared.
bool GetSkinMatrix(out mat4 skin)
{
    uint base = paletteBases.bases[gl_InstanceIndex];
    if (base == NO_SKIN || dot(inBoneWeights, vec4(1.0)) <= 0.0)
        return false;

    skin = inBoneWeights.x * bonePalette.bones[base + inBoneIDs.x]
         + inBoneWeights.y * bonePalette.bones[base + inBoneIDs.y]
         + inBoneWeights.z * bonePalette.bones[base + inBoneIDs.z]
         + inBoneWeights.w * bonePalette.bones[base + inBoneIDs.w];
    return true;
}

void main()
{
    vec4 position = vec4(inPosition, 1.0);

    // POSITION ONLY. This shader declares inNormal but never reads it, and its fragment
    // stage is a flat colour -- skinning the normal here would be dead code, exactly as
    // it was in BuiltIn.MaterialShader before lighting existed.
    mat4 skin;
    if (GetSkinMatrix(skin))
        position = skin * position;

    gl_Position = globalUBO.projection * globalUBO.view
                * instanceData.models[gl_InstanceIndex] * position;
}

// -------------------------------------------------------------------

#pragma stage fragment
#version 450

layout(location = 0) out vec4 fragColor;

struct DirectionalLight { vec4 direction; vec4 color; };
struct PointLight        { vec4 position; vec4 color; };
struct SpotLight         { vec4 position; vec4 direction; vec4 color; vec4 angles; };

layout(set = 0, binding = 0) uniform GlobalUBO
{
    mat4             projection;
    mat4             view;
    vec4             viewPosition;
    vec4             ambientColor;
    DirectionalLight dirLight;
    ivec4            lightCountAndPad;
    PointLight       pointLights[16];
    vec4             time;  // x=totalTime, y=sin(t), z=cos(t), w=deltaTime
    ivec4            spotLightCountAndPad;
    SpotLight        spotLights[8];
} globalUBO;

layout(set = 1, binding = 0) uniform InstanceUBO
{
    vec4 diffuseColor;  // rgb = tint, a = opacity (unused)
} instanceUBO;

void main()
{
    fragColor = instanceUBO.diffuseColor;
}
