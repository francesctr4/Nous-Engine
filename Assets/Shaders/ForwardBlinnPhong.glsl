#pragma stage vertex
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;

// Data Transfer Object
layout(location = 0) out struct DataTransferObject
{
    vec3 outColor;
    vec2 texCoord;
    vec3 fragPos;
    vec3 normal;
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
} globalUBO;

layout(push_constant) uniform PushConstants
{
    // Only guaranteed a total of 128 bytes.
    mat4 model; // 64 bytes
} pushConstants;

void main()
{
    vec4 worldPos     = pushConstants.model * vec4(inPosition, 1.0);
    outDTO.outColor   = inColor;
    outDTO.texCoord   = inTexCoord;
    outDTO.fragPos    = worldPos.xyz;
    outDTO.normal     = mat3(transpose(inverse(pushConstants.model))) * inNormal;
    gl_Position       = globalUBO.projection * globalUBO.view * worldPos;
}

// ------------------------------------------------------------------------------------------------------

#pragma stage fragment
#version 450

// Data Transfer Object
layout(location = 0) in struct DataTransferObject
{
    vec3 outColor;
    vec2 texCoord;
    vec3 fragPos;
    vec3 normal;
} inDTO;

layout(location = 0) out vec4 fragColor;

// Instance data (set = 1)
layout(set = 1, binding = 0) uniform InstanceUBO
{
    vec4 diffuseColor;
} instanceUBO;

layout(set = 1, binding = 1) uniform sampler2D diffuseSampler;
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
} globalUBO;

// ── Blinn-Phong helpers ───────────────────────────────────────────────────────

vec3 CalcDirectionalLight(vec4 dirAndUnused, vec4 colorAndIntensity,
                          vec3 normal, vec3 viewDir,
                          vec3 albedo, float specStrength, float shininess)
{
    vec3  lightDir = normalize(-dirAndUnused.xyz);
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
    float specStrength = texture(specularSampler,   uv).r;
    float shininess    = max(texture(shininessSampler, uv).r * 256.0, 1.0);
    float ao           = texture(aoSampler,         uv).r;
    vec3  emissive     = texture(emissiveSampler,   uv).rgb;

    // --- NORMAL MAP (object-space; TBN transform deferred to future task) ---
    vec3 normal  = normalize(texture(normalSampler, uv).rgb * 2.0 - 1.0);
    vec3 viewDir = normalize(globalUBO.viewPosition.xyz - inDTO.fragPos);

    // --- AMBIENT ---
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
