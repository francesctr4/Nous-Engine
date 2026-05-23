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
    vec4  albedoColor;    // rgb = tint, a unused
    vec4  emissiveColor;  // rgb = color, a = intensity
    float metallic;       // metallic factor [0,1]
    float roughness;      // roughness factor [0,1]
    float aoStrength;     // ambient occlusion multiplier [0,1]
    float normalStrength; // normal map XY scale
} instanceUBO;

layout(set = 1, binding = 1) uniform sampler2D albedoSampler;
layout(set = 1, binding = 2) uniform sampler2D normalSampler;
layout(set = 1, binding = 3) uniform sampler2D metallicSampler;
layout(set = 1, binding = 4) uniform sampler2D roughnessSampler;
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

const float PI = 3.14159265359;

// ── Cook-Torrance BRDF helpers ────────────────────────────────────────────────

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness * roughness;
    float a2     = a * a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom  = NdotH2 * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// ── PBR radiance contribution for a single light ──────────────────────────────

vec3 CalcPBRLight(vec3 L, vec3 radiance,
                  vec3 N, vec3 V, vec3 albedo,
                  float metallic, float roughness, vec3 F0)
{
    vec3  H    = normalize(V + L);
    float NDF  = DistributionGGX(N, H, roughness);
    float G    = GeometrySmith(N, V, L, roughness);
    vec3  F    = FresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3  num   = NDF * G * F;
    float denom = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3  spec  = num / denom;

    // kD: energy conservation — metals have no diffuse
    vec3  kD    = (vec3(1.0) - F) * (1.0 - metallic);
    float NdotL = max(dot(N, L), 0.0);
    return (kD * albedo / PI + spec) * radiance * NdotL;
}

vec3 CalcSpotLight_PBR(SpotLight light, vec3 fragPos, vec3 N, vec3 V,
                       vec3 albedo, float metallic, float roughness, vec3 F0)
{
    vec3  toLight  = light.position.xyz - fragPos;
    float dist     = length(toLight);
    if (dist >= light.position.w) return vec3(0.0);

    vec3  lightDir = normalize(toLight);
    float cosTheta = dot(-lightDir, normalize(light.direction.xyz));

    float epsilon   = light.angles.x - light.angles.y;
    float spotAtten = clamp((cosTheta - light.angles.y) / epsilon, 0.0, 1.0);

    float distAtten = clamp(1.0 - (dist / light.position.w), 0.0, 1.0);
    distAtten      *= distAtten;

    vec3 L        = lightDir;
    vec3 radiance = light.color.rgb * light.color.w * spotAtten * distAtten;
    return CalcPBRLight(L, radiance, N, V, albedo, metallic, roughness, F0);
}

// ── Main ─────────────────────────────────────────────────────────────────────

void main()
{
    vec2 uv = inDTO.texCoord;

    // --- SAMPLE TEXTURES ---
    // Albedo is authored in sRGB; convert to linear for PBR math.
    vec3  albedo    = pow(texture(albedoSampler,    uv).rgb, vec3(2.2)) * instanceUBO.albedoColor.rgb;
    float metallic  = texture(metallicSampler,  uv).r * instanceUBO.metallic;
    float roughness = max(texture(roughnessSampler, uv).r * instanceUBO.roughness, 0.04); // clamp to avoid specular singularity
    float ao        = mix(1.0, texture(aoSampler, uv).r, instanceUBO.aoStrength);
    vec3  emissive  = texture(emissiveSampler,  uv).rgb * instanceUBO.emissiveColor.rgb * instanceUBO.emissiveColor.a;

    // --- NORMAL MAP (tangent space → world space via TBN) ---
    mat3 TBN = mat3(normalize(inDTO.T), normalize(inDTO.B), normalize(inDTO.N));
    vec3 normalSample = texture(normalSampler, uv).rgb * 2.0 - 1.0; // [0,1] → [-1,1]
    normalSample.xy  *= instanceUBO.normalStrength;
    vec3 N = normalize(TBN * normalSample); // tangent space → world space

    vec3 V = normalize(globalUBO.viewPosition.xyz - inDTO.fragPos);

    // F0: dielectric baseline is 0.04; metals use albedo color as F0
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3 Lo = vec3(0.0);

    // --- DIRECTIONAL LIGHT ---
    {
        vec3 L        = normalize(-globalUBO.dirLight.direction.xyz); // direction points toward the scene
        vec3 radiance = globalUBO.dirLight.color.rgb * globalUBO.dirLight.color.w;
        Lo += CalcPBRLight(L, radiance, N, V, albedo, metallic, roughness, F0);
    }

    // --- POINT LIGHTS ---
    for (int i = 0; i < globalUBO.lightCountAndPad.x; i++)
    {
        vec3  toLight = globalUBO.pointLights[i].position.xyz - inDTO.fragPos;
        float dist    = length(toLight);
        float range   = globalUBO.pointLights[i].position.w;
        if (dist >= range) continue;

        float atten   = clamp(1.0 - (dist / range), 0.0, 1.0);
        atten        *= atten; // quadratic falloff

        vec3 L        = normalize(toLight);
        vec3 radiance = globalUBO.pointLights[i].color.rgb * globalUBO.pointLights[i].color.w * atten;
        Lo += CalcPBRLight(L, radiance, N, V, albedo, metallic, roughness, F0);
    }

    // --- SPOT LIGHTS ---
    for (int i = 0; i < globalUBO.spotLightCountAndPad.x; i++)
    {
        Lo += CalcSpotLight_PBR(globalUBO.spotLights[i],
                                inDTO.fragPos, N, V,
                                albedo, metallic, roughness, F0);
    }

    // --- AMBIENT (simple constant-color; IBL deferred to Phase 4) ---
    vec3 ambient = globalUBO.ambientColor.rgb * globalUBO.ambientColor.w * albedo * ao;

    vec3 color = ambient + Lo + emissive;

    // --- HDR tonemapping (Reinhard) + gamma correction ---
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    fragColor = vec4(color, 1.0);
}
