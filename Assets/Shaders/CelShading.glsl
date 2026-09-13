#pragma stage vertex
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;
// location 4 = smoothNormal (outline only, not used here)
layout(location = 5) in vec4 inTangent; // xyz = tangent direction, w = bitangent handedness sign (±1)
layout(location = 7) in uvec4 inBoneIDs;
layout(location = 8) in vec4  inBoneWeights;

layout(location = 0) out struct DataTransferObject
{
    vec3 outColor;
    vec2 texCoord;
    vec3 fragPos;  // world-space fragment position
    vec3 T;        // world-space tangent
    vec3 B;        // world-space bitangent
    vec3 N;        // world-space geometry normal
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
    vec4             time;
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
    mat4 model = instanceData.models[gl_InstanceIndex];

    vec3 position = inPosition;
    vec3 normal   = inNormal;
    vec3 tangent  = inTangent.xyz;

    mat4 skin;
    if (GetSkinMatrix(skin))
    {
        position = (skin * vec4(position, 1.0)).xyz;

        // The TANGENT is skinned too, not just the normal. This shader samples normal
        // maps through the TBN, so a bind-pose tangent with a skinned normal gives an
        // orthogonal-looking but wrong basis, and mapped detail slides as the character
        // moves. The bitangent needs nothing -- it is cross(N, T) * w, and the
        // handedness sign is a property of the UVs, not the pose.
        //
        // mat3(skin) is correct for rigid and uniformly-scaled bones; a non-uniformly
        // scaled bone would need the inverse transpose. No rig here uses one, and
        // SkinVertices documents the identical caveat.
        normal  = mat3(skin) * normal;
        tangent = mat3(skin) * tangent;
    }

    vec4 worldPos   = model * vec4(position, 1.0);
    outDTO.outColor = inColor;
    outDTO.texCoord = inTexCoord;
    outDTO.fragPos  = worldPos.xyz;

    // Skinning happens in MODEL space, so this model->world normal matrix is unchanged
    // and simply receives skinned inputs.
    mat3 normalMat = mat3(transpose(inverse(model)));
    vec3 N = normalize(normalMat * normal);
    vec3 T = normalize(mat3(model) * tangent);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T) * inTangent.w;

    outDTO.T = T;
    outDTO.B = B;
    outDTO.N = N;

    gl_Position = globalUBO.projection * globalUBO.view * worldPos;
}

// ------------------------------------------------------------------------------------------------------

#pragma stage fragment
#version 450

layout(location = 0) in struct DataTransferObject
{
    vec3 outColor;
    vec2 texCoord;
    vec3 fragPos;
    vec3 T;
    vec3 B;
    vec3 N;
} inDTO;

layout(location = 0) out vec4 fragColor;

// Same InstanceUBO layout as ForwardBlinnPhong — materials are cross-compatible.
layout(set = 1, binding = 0) uniform InstanceUBO
{
    vec4  diffuseColor;
    vec4  emissiveColor;
    float aoIntensity;
    float normalStrength;
    float specularIntensity;
    float shininessScale;   // higher = smaller/tighter specular blob
} instanceUBO;

layout(set = 1, binding = 1) uniform sampler2D diffuseSampler;
layout(set = 1, binding = 2) uniform sampler2D normalSampler;
layout(set = 1, binding = 3) uniform sampler2D specularSampler;
layout(set = 1, binding = 4) uniform sampler2D shininessSampler;
layout(set = 1, binding = 5) uniform sampler2D aoSampler;
layout(set = 1, binding = 6) uniform sampler2D emissiveSampler;

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
    vec4             time;
    ivec4            spotLightCountAndPad;
    SpotLight        spotLights[8];
} globalUBO;

// ── Cel-shading helpers ───────────────────────────────────────────────────────

// Three hard diffuse bands: fully unlit / mid-tone / fully lit.
// The shadow band (NdotL < 0.05) returns 0 so only ambient reaches it,
// producing the very dark "ink" shadows characteristic of Borderlands.
float CelDiffuse(float NdotL)
{
    if (NdotL < 0.05)  return 0.0;
    if (NdotL < 0.45)  return 0.45;
    return 1.0;
}

// Binary specular: the highlight either exists or it doesn't.
// shininess controls blob size — higher shininess = pow result falls off faster = smaller blob.
float CelSpecular(float NdotH, float shininess)
{
    float s = pow(max(NdotH, 0.0), shininess);
    return step(0.5, s);
}

// ── Main ─────────────────────────────────────────────────────────────────────

void main()
{
    vec2 uv = inDTO.texCoord;

    // --- TEXTURES ---
    vec3  albedo       = texture(diffuseSampler,    uv).rgb * instanceUBO.diffuseColor.rgb;
    float specStrength = texture(specularSampler,   uv).r   * instanceUBO.specularIntensity;
    float shininess    = max(texture(shininessSampler, uv).r * 256.0 * instanceUBO.shininessScale, 1.0);
    float ao           = mix(1.0, texture(aoSampler, uv).r,  instanceUBO.aoIntensity);
    vec3  emissive     = texture(emissiveSampler,   uv).rgb  * instanceUBO.emissiveColor.rgb * instanceUBO.emissiveColor.a;

    // --- NORMAL MAP (tangent space → world space via TBN) ---
    mat3 TBN = mat3(normalize(inDTO.T), normalize(inDTO.B), normalize(inDTO.N));
    vec3 normalSample = texture(normalSampler, uv).rgb * 2.0 - 1.0;
    normalSample.xy  *= instanceUBO.normalStrength;
    vec3 N = normalize(TBN * normalSample);

    vec3 viewDir = normalize(globalUBO.viewPosition.xyz - inDTO.fragPos);

    // --- AMBIENT ---
    vec3 result = globalUBO.ambientColor.rgb * globalUBO.ambientColor.w * albedo * ao;

    // --- DIRECTIONAL LIGHT ---
    {
        vec3  lightDir  = normalize(-globalUBO.dirLight.direction.xyz);
        float NdotL     = max(dot(N, lightDir), 0.0);
        float cel       = CelDiffuse(NdotL);

        vec3  halfway   = normalize(lightDir + viewDir);
        float celSpec   = CelSpecular(dot(N, halfway), shininess) * specStrength;

        vec3 lightColor = globalUBO.dirLight.color.rgb * globalUBO.dirLight.color.w;
        result += cel * albedo * lightColor + celSpec * lightColor;
    }

    // --- POINT LIGHTS ---
    for (int i = 0; i < globalUBO.lightCountAndPad.x; i++)
    {
        vec3  toLight = globalUBO.pointLights[i].position.xyz - inDTO.fragPos;
        float dist    = length(toLight);
        float range   = globalUBO.pointLights[i].position.w;
        if (dist >= range) continue;

        float atten   = clamp(1.0 - (dist / range), 0.0, 1.0);
        atten        *= atten;

        vec3  lightDir  = normalize(toLight);
        float NdotL     = max(dot(N, lightDir), 0.0);
        float cel       = CelDiffuse(NdotL);

        vec3  halfway   = normalize(lightDir + viewDir);
        float celSpec   = CelSpecular(dot(N, halfway), shininess) * specStrength;

        vec3 lightColor = globalUBO.pointLights[i].color.rgb * globalUBO.pointLights[i].color.w * atten;
        result += (cel * albedo + celSpec) * lightColor;
    }

    // --- SPOT LIGHTS ---
    for (int i = 0; i < globalUBO.spotLightCountAndPad.x; i++)
    {
        vec3  toLight = globalUBO.spotLights[i].position.xyz - inDTO.fragPos;
        float dist    = length(toLight);
        float range   = globalUBO.spotLights[i].position.w;
        if (dist >= range) continue;

        float atten     = clamp(1.0 - (dist / range), 0.0, 1.0);
        atten          *= atten;
        vec3  lightDir  = normalize(toLight);
        float cosTheta  = dot(-lightDir, normalize(globalUBO.spotLights[i].direction.xyz));
        float cosInner  = globalUBO.spotLights[i].angles.x;
        float cosOuter  = globalUBO.spotLights[i].angles.y;
        float spotFade  = clamp((cosTheta - cosOuter) / (cosInner - cosOuter), 0.0, 1.0);
        if (spotFade <= 0.0) continue;

        float NdotL    = max(dot(N, lightDir), 0.0);
        float cel      = CelDiffuse(NdotL);
        vec3  halfway  = normalize(lightDir + viewDir);
        float celSpec  = CelSpecular(dot(N, halfway), shininess) * specStrength;
        vec3  color    = globalUBO.spotLights[i].color.rgb * globalUBO.spotLights[i].color.w * atten * spotFade;
        result += (cel * albedo + celSpec) * color;
    }

    // --- RIM LIGHT ---
    // Hard band at silhouette edges — reinforces the painted/outlined Borderlands look.
    float rim = 1.0 - max(dot(N, viewDir), 0.0);
    result += step(0.6, rim) * step(rim, 0.9) * albedo * 0.3;

    result += emissive;

    fragColor = vec4(result, 1.0);
}
