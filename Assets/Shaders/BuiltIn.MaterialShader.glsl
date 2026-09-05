#pragma stage vertex
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;

// Skinning inputs. These need NO C++ change: k_Vertex3DOffsets in VulkanShader.cpp
// already carries locations 7 and 8, and vertex attributes are built from shader
// reflection. Static geometry writes all-zero weights, so one Vertex3D and one
// pipeline serve both skinned and unskinned meshes.
layout(location = 7) in uvec4 inBoneIDs;
layout(location = 8) in vec4  inBoneWeights;

// Data Transfer Object
layout(location = 0) out struct DataTransferObject
{
    vec3 outColor;
    vec2 texCoord;
} outDTO;

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

// Per-instance bone-palette base, parallel to instanceData.models. NO_SKIN means
// this instance is not skinned.
layout(set = 0, binding = 2) readonly buffer PaletteBases
{
    uint bases[];
} paletteBases;

// Every skinned instance's bone palette, concatenated. Indexed as
// bones[base + boneID], which is why two characters sharing a mesh and material
// can stay in one instanced draw while holding different poses.
layout(set = 0, binding = 3) readonly buffer BonePalette
{
    mat4 bones[];
} bonePalette;

const uint NO_SKIN = 0xFFFFFFFFu;

void main()
{
    outDTO.outColor = inColor;
    outDTO.texCoord = inTexCoord;

    vec4 position = vec4(inPosition, 1.0);
    vec3 normal   = inNormal;

    // Two guards, covering different failures.
    //
    // The SENTINEL covers a rigged mesh whose animator has not bound yet: its
    // weights are non-zero, so a weights-only test would index into a palette that
    // was never uploaded.
    //
    // The WEIGHT TEST covers an unweighted vertex inside a skinned mesh, which
    // would otherwise accumulate a zero matrix and collapse to the origin.
    //
    // These are the same two rules AnimationSystem's SkinVertices implements, so
    // the GPU path and the tested CPU reference agree by construction.
    uint base = paletteBases.bases[gl_InstanceIndex];
    if (base != NO_SKIN && dot(inBoneWeights, vec4(1.0)) > 0.0)
    {
        mat4 skin = inBoneWeights.x * bonePalette.bones[base + inBoneIDs.x]
                  + inBoneWeights.y * bonePalette.bones[base + inBoneIDs.y]
                  + inBoneWeights.z * bonePalette.bones[base + inBoneIDs.z]
                  + inBoneWeights.w * bonePalette.bones[base + inBoneIDs.w];
        position = skin * position;

        // Correct for rigid and uniformly-scaled bones; wrong for non-uniform bone
        // scale, which needs the inverse transpose. No rig here uses one, and
        // SkinVertices documents the identical caveat.
        //
        // Still visually inert -- this shader is unlit and never reads the normal --
        // but no longer UNVERIFIABLE: SkinVertices computes the same quantity on the
        // CPU and the normals debug visualization displays it.
        normal = normalize(mat3(skin) * normal);
    }

    gl_Position = globalUBO.projection * globalUBO.view
                * instanceData.models[gl_InstanceIndex] * position;
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
    vec4 diffuseColor;     // rgb = tint, a = opacity (unused)
} instanceUBO;

// Samplers
layout(set = 1, binding = 1) uniform sampler2D diffuseSampler;

void main()
{
    vec4 diffuse = instanceUBO.diffuseColor * texture(diffuseSampler, inDTO.texCoord);
    fragColor = diffuse;
}
