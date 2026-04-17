#pragma stage vertex
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;
// location 4 = smoothNormal (outline only, not used here)
layout(location = 5) in vec4 inTangent; // xyz = tangent direction, w = bitangent handedness sign (±1)

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
} globalUBO;

layout(set = 0, binding = 1) readonly buffer InstanceData
{
    mat4 models[];
} instanceData;

void main()
{
    mat4 model      = instanceData.models[gl_InstanceIndex];
    vec4 worldPos   = model * vec4(inPosition, 1.0);
    outDTO.outColor = inColor;
    outDTO.texCoord = inTexCoord;
    outDTO.fragPos  = worldPos.xyz;

    // Build world-space TBN.
    // The normal matrix (transpose of inverse) handles non-uniform scaling correctly.
    // Tangent is a direction vector and transforms with the model matrix directly.
    mat3 normalMat = mat3(transpose(inverse(model)));
    vec3 N = normalize(normalMat * inNormal);
    vec3 T = normalize(mat3(model) * inTangent.xyz);
    // Gram-Schmidt re-orthogonalization: removes any drift from non-orthogonal transforms.
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T) * inTangent.w; // handedness sign flips B for mirrored UVs

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

// Instance data (set = 1)
layout(set = 1, binding = 0) uniform InstanceUBO
{
    vec4  diffuseColor;       // rgb = tint, a = opacity (unused)
    vec4  emissiveColor;      // rgb = color, a = intensity
    float aoIntensity;        // ambient occlusion multiplier [0,1]
    float normalStrength;     // normal map XY scale
    float specularIntensity;  // specular reflectance multiplier
    float shininessScale;     // shininess (gloss) multiplier
} instanceUBO;

layout(set = 1, binding = 1) uniform sampler2D diffuseSampler;
// Normal map must be a tangent-space map (OpenGL convention: +Y = up in tangent space).
// The engine falls back to a white texture when no normal map is assigned; to get correct
// geometry-normal shading in that case, replace the fallback with a flat normal map
// (RGB = 128, 128, 255) in the resource manager.
layout(set = 1, binding = 2) uniform sampler2D normalSampler;
layout(set = 1, binding = 3) uniform sampler2D specularSampler;
layout(set = 1, binding = 4) uniform sampler2D shininessSampler;
layout(set = 1, binding = 5) uniform sampler2D aoSampler;
layout(set = 1, binding = 6) uniform sampler2D emissiveSampler;

struct DirectionalLight { vec4 direction; vec4 color; };
struct PointLight        { vec4 position; vec4 color; };

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
} globalUBO;

// ── Blinn-Phong helpers ───────────────────────────────────────────────────────

vec3 CalcDirectionalLight(vec4 dirAndUnused, vec4 colorAndIntensity,
                          vec3 normal, vec3 viewDir,
                          vec3 albedo, float specStrength, float shininess)
{
    vec3  lightDir = normalize(-dirAndUnused.xyz); // dirAndUnused points toward the scene
    float diff     = max(dot(normal, lightDir), 0.0);
    vec3  halfway  = normalize(lightDir + viewDir);
    float spec     = pow(max(dot(normal, halfway), 0.0), shininess);
    vec3  diffuse  = diff * albedo * colorAndIntensity.rgb * colorAndIntensity.w;
    vec3  specular = spec * specStrength * colorAndIntensity.rgb * colorAndIntensity.w;
    return diffuse + specular;
}

vec3 CalcPointLight(vec4 posAndRange, vec4 colorAndIntensity,
                    vec3 fragPos, vec3 normal, vec3 viewDir,
                    vec3 albedo, float specStrength, float shininess)
{
    vec3  toLight = posAndRange.xyz - fragPos;
    float dist    = length(toLight);
    float range   = posAndRange.w;
    if (dist >= range) return vec3(0.0);

    float atten   = clamp(1.0 - (dist / range), 0.0, 1.0);
    atten        *= atten; // quadratic falloff

    vec3  lightDir = normalize(toLight);
    float diff     = max(dot(normal, lightDir), 0.0);
    vec3  halfway  = normalize(lightDir + viewDir);
    float spec     = pow(max(dot(normal, halfway), 0.0), shininess);

    vec3 diffuse  = diff * albedo * colorAndIntensity.rgb * colorAndIntensity.w;
    vec3 specular = spec * specStrength * colorAndIntensity.rgb * colorAndIntensity.w;
    return (diffuse + specular) * atten;
}

// ── Main ─────────────────────────────────────────────────────────────────────

void main()
{
    vec2 uv = inDTO.texCoord;

    // --- SAMPLE TEXTURES ---
    vec3  albedo       = texture(diffuseSampler,    uv).rgb * instanceUBO.diffuseColor.rgb;
    float specStrength = texture(specularSampler,   uv).r * instanceUBO.specularIntensity;
    float shininess    = max(texture(shininessSampler, uv).r * 256.0 * instanceUBO.shininessScale, 1.0);
    float ao           = mix(1.0, texture(aoSampler, uv).r, instanceUBO.aoIntensity);
    vec3  emissive     = texture(emissiveSampler,   uv).rgb * instanceUBO.emissiveColor.rgb * instanceUBO.emissiveColor.a;

    // --- NORMAL MAP (tangent space → world space via TBN) ---
    // Reconstruct the TBN matrix from the interpolated per-vertex vectors.
    // Each column is re-normalized after interpolation to remove length drift.
    mat3 TBN = mat3(normalize(inDTO.T), normalize(inDTO.B), normalize(inDTO.N));
    vec3 normalSample = texture(normalSampler, uv).rgb * 2.0 - 1.0; // [0,1] → [-1,1]
    normalSample.xy  *= instanceUBO.normalStrength;
    vec3 normal       = normalize(TBN * normalSample); // tangent space → world space

    vec3 viewDir = normalize(globalUBO.viewPosition.xyz - inDTO.fragPos);

    // --- AMBIENT (AO only attenuates ambient, not direct lights) ---
    vec3 result = globalUBO.ambientColor.rgb * globalUBO.ambientColor.w * albedo * ao;

    // --- DIRECTIONAL LIGHT ---
    result += CalcDirectionalLight(globalUBO.dirLight.direction,
                                   globalUBO.dirLight.color,
                                   normal, viewDir,
                                   albedo, specStrength, shininess);

    // --- POINT LIGHTS ---
    for (int i = 0; i < globalUBO.lightCountAndPad.x; i++)
    {
        result += CalcPointLight(globalUBO.pointLights[i].position,
                                 globalUBO.pointLights[i].color,
                                 inDTO.fragPos, normal, viewDir,
                                 albedo, specStrength, shininess);
    }

    result += emissive;

    fragColor = vec4(result, 1.0);
}

