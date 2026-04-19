#pragma stage vertex
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;
// location 4 = smoothNormal (outline only, not used here)
layout(location = 5) in vec4 inTangent;

layout(location = 0) out struct DataTransferObject
{
    vec3 outColor;
    vec2 texCoord;
    vec3 fragPos;
    vec3 T;
    vec3 B;
    vec3 N;
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
    mat4 model    = instanceData.models[gl_InstanceIndex];
    vec4 worldPos = model * vec4(inPosition, 1.0);

    mat3 normalMat = mat3(transpose(inverse(model)));
    vec3 N = normalize(normalMat * inNormal);
    vec3 T = normalize(mat3(model) * inTangent.xyz);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T) * inTangent.w;

    // Animate the mesh surface: sine wave rides up the Y axis while a cosine
    // ripples across X, producing a slow "breathing" undulation.
    float t    = globalUBO.time.x;
    float wave = sin(worldPos.y * 5.0 + t * 2.0) * cos(worldPos.x * 3.7 + t * 1.3) * 0.015;
    worldPos.xyz += N * wave;

    outDTO.outColor = inColor;
    outDTO.texCoord = inTexCoord;
    outDTO.fragPos  = worldPos.xyz;
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

layout(set = 1, binding = 0) uniform InstanceUBO
{
    vec4  diffuseColor;
    vec4  emissiveColor;
    float aoIntensity;
    float normalStrength;
    float specularIntensity;
    float shininessScale;
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

// ── Helpers ───────────────────────────────────────────────────────────────────

// Full-spectrum rainbow from a [0,1] hue — smooth, no banding.
vec3 Rainbow(float hue)
{
    vec3 k = vec3(0.0, 1.0 / 3.0, 2.0 / 3.0);
    vec3 p = abs(fract(hue + k) * 6.0 - 3.0);
    return clamp(p - 1.0, 0.0, 1.0);
}

// Blinn-Phong direct lighting helpers (same as ForwardBlinnPhong).
vec3 CalcDirectionalLight(vec4 dir, vec4 col,
                          vec3 N, vec3 V,
                          vec3 albedo, float spec, float shin)
{
    vec3  L    = normalize(-dir.xyz);
    float diff = max(dot(N, L), 0.0);
    vec3  H    = normalize(L + V);
    float sp   = pow(max(dot(N, H), 0.0), shin);
    return diff * albedo * col.rgb * col.w + sp * spec * col.rgb * col.w;
}

vec3 CalcPointLight(vec4 posRange, vec4 col,
                    vec3 fragPos, vec3 N, vec3 V,
                    vec3 albedo, float spec, float shin)
{
    vec3  toL  = posRange.xyz - fragPos;
    float dist = length(toL);
    if (dist >= posRange.w) return vec3(0.0);
    float att  = clamp(1.0 - dist / posRange.w, 0.0, 1.0);
    att *= att;
    vec3  L    = normalize(toL);
    float diff = max(dot(N, L), 0.0);
    vec3  H    = normalize(L + V);
    float sp   = pow(max(dot(N, H), 0.0), shin);
    return (diff * albedo * col.rgb * col.w + sp * spec * col.rgb * col.w) * att;
}

// ── Main ─────────────────────────────────────────────────────────────────────

void main()
{
    float t  = globalUBO.time.x;
    vec2  uv = inDTO.texCoord;

    // ── Textures ──────────────────────────────────────────────────────────────
    vec3  albedo       = texture(diffuseSampler,    uv).rgb * instanceUBO.diffuseColor.rgb;
    float specStrength = texture(specularSampler,   uv).r   * instanceUBO.specularIntensity;
    float shininess    = max(texture(shininessSampler, uv).r * 256.0 * instanceUBO.shininessScale, 1.0);
    float ao           = mix(1.0, texture(aoSampler,   uv).r, instanceUBO.aoIntensity);
    vec3  emissive     = texture(emissiveSampler,   uv).rgb  * instanceUBO.emissiveColor.rgb
                       * instanceUBO.emissiveColor.a;

    // ── Normal map ────────────────────────────────────────────────────────────
    mat3 TBN = mat3(normalize(inDTO.T), normalize(inDTO.B), normalize(inDTO.N));
    vec3 ns  = texture(normalSampler, uv).rgb * 2.0 - 1.0;
    ns.xy   *= instanceUBO.normalStrength;
    vec3 N   = normalize(TBN * ns);
    vec3 V   = normalize(globalUBO.viewPosition.xyz - inDTO.fragPos);

    // ── Base Blinn-Phong lighting ─────────────────────────────────────────────
    vec3 ambient = globalUBO.ambientColor.rgb * globalUBO.ambientColor.w * albedo * ao;
    vec3 direct  = CalcDirectionalLight(globalUBO.dirLight.direction, globalUBO.dirLight.color,
                                        N, V, albedo, specStrength, shininess);
    for (int i = 0; i < globalUBO.lightCountAndPad.x; i++)
        direct += CalcPointLight(globalUBO.pointLights[i].position, globalUBO.pointLights[i].color,
                                 inDTO.fragPos, N, V, albedo, specStrength, shininess);
    for (int i = 0; i < globalUBO.spotLightCountAndPad.x; i++)
    {
        SpotLight sl    = globalUBO.spotLights[i];
        vec3  toL       = sl.position.xyz - inDTO.fragPos;
        float dist      = length(toL);
        if (dist >= sl.position.w) continue;
        float att       = clamp(1.0 - dist / sl.position.w, 0.0, 1.0); att *= att;
        vec3  L         = normalize(toL);
        float cosTheta  = dot(-L, normalize(sl.direction.xyz));
        float spotFade  = clamp((cosTheta - sl.angles.y) / (sl.angles.x - sl.angles.y), 0.0, 1.0);
        if (spotFade <= 0.0) continue;
        direct += CalcPointLight(sl.position, sl.color, inDTO.fragPos, N, V, albedo, specStrength, shininess) * spotFade;
    }

    vec3 baseLight = ambient + direct;

    // ── Iridescence ───────────────────────────────────────────────────────────
    // View-angle drives the hue so the rainbow shifts as you orbit the camera.
    // Time slowly rolls the phase so colors drift even when you stand still.
    float NdotV    = max(dot(N, V), 0.0);
    float iriHue   = fract(NdotV * 2.5 + t * 0.12);
    vec3  iriColor = Rainbow(iriHue);

    // Blend strength: stronger on grazing angles (Fresnel-like), always at least 40%.
    float iriBlend = mix(0.40, 0.85, 1.0 - NdotV);
    vec3  result   = mix(baseLight, baseLight * iriColor * 1.4, iriBlend);

    // ── Scrolling scanlines ───────────────────────────────────────────────────
    // Horizontal bands in world-Y that scroll upward — holographic display feel.
    float scan = 0.80 + 0.20 * step(0.5, fract(inDTO.fragPos.y * 40.0 - t * 0.8));
    result    *= scan;

    // ── Glitter sparkles ──────────────────────────────────────────────────────
    // Each surface point has a pseudo-random phase driven by its world position.
    // Multiplied by a fast time oscillation: sparkles flash in and out rapidly.
    float sparklePhase = dot(inDTO.fragPos, vec3(127.1, 311.7, 74.7));
    float sparkle      = pow(max(0.0, sin(sparklePhase + t * 8.0)), 28.0)
                       * pow(max(0.0, sin(sparklePhase * 0.37 + t * 5.3)), 8.0);
    result += vec3(sparkle) * 4.0;   // hot-white flash

    // ── Pulsing rainbow rim ───────────────────────────────────────────────────
    // Fresnel rim in a hue offset from the surface iridescence, pulsing with time.
    float rim      = pow(1.0 - NdotV, 3.0);
    float rimPulse = 0.5 + 0.5 * sin(t * 2.5);
    float rimHue   = fract(iriHue + 0.5);            // complementary hue
    result        += Rainbow(rimHue) * rim * rimPulse * 1.8;

    result += emissive;

    fragColor = vec4(result, 1.0);
}
