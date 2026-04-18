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

    mat3 normalMat = mat3(transpose(inverse(model)));
    vec3 N = normalize(normalMat * inNormal);
    vec3 T = normalize(mat3(model) * inTangent.xyz);
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

layout(set = 1, binding = 0) uniform InstanceUBO
{
    vec4  diffuseColor;   // base color tint
    vec4  edgeColor;      // rgb = fire/glow color, a = edge glow intensity
    float dissolveSpeed;  // full fade cycles per second (try 0.15 - 0.4)
    float noiseScale;     // noise UV frequency — higher = finer grain (try 3 - 7)
    float edgeWidth;      // fire edge thickness as a noise fraction (try 0.05 - 0.12)
    float normalStrength; // normal map XY scale
} instanceUBO;

layout(set = 1, binding = 1) uniform sampler2D diffuseSampler;
layout(set = 1, binding = 2) uniform sampler2D normalSampler;

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
    vec4             time;
} globalUBO;

// ── Noise ─────────────────────────────────────────────────────────────────────

float Hash(vec2 p)
{
    p  = fract(p * vec2(234.34, 435.345));
    p += dot(p, p + 34.23);
    return fract(p.x * p.y);
}

float ValueNoise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f); // smoothstep

    return mix(mix(Hash(i),               Hash(i + vec2(1.0, 0.0)), u.x),
               mix(Hash(i + vec2(0.0, 1.0)), Hash(i + vec2(1.0, 1.0)), u.x), u.y);
}

// 5-octave FBM — gives the organic, cloud-like dissolve pattern.
float FBM(vec2 p)
{
    float v = 0.0, a = 0.5;
    mat2  rot = mat2(cos(0.5), sin(0.5), -sin(0.5), cos(0.5)); // slight rotation per octave
    for (int i = 0; i < 5; ++i)
    {
        v += a * ValueNoise(p);
        p  = rot * p * 2.1;
        a *= 0.5;
    }
    return v;
}

// ── Lighting helpers (Blinn-Phong) ────────────────────────────────────────────

vec3 CalcDirectionalLight(vec4 dir, vec4 col, vec3 N, vec3 V, vec3 albedo)
{
    vec3  L    = normalize(-dir.xyz);
    float diff = max(dot(N, L), 0.0);
    vec3  H    = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), 32.0) * 0.3;
    return (diff * albedo + spec) * col.rgb * col.w;
}

vec3 CalcPointLight(vec4 posRange, vec4 col, vec3 fragPos, vec3 N, vec3 V, vec3 albedo)
{
    vec3  toL  = posRange.xyz - fragPos;
    float dist = length(toL);
    if (dist >= posRange.w) return vec3(0.0);
    float att  = clamp(1.0 - dist / posRange.w, 0.0, 1.0);
    att *= att;
    vec3  L    = normalize(toL);
    float diff = max(dot(N, L), 0.0);
    vec3  H    = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), 32.0) * 0.3;
    return (diff * albedo + spec) * col.rgb * col.w * att;
}

// ── Main ─────────────────────────────────────────────────────────────────────

void main()
{
    float t   = globalUBO.time.x;
    vec2  uv  = inDTO.texCoord;

    // ── Dissolve noise ────────────────────────────────────────────────────────
    // Drift UVs slowly so the pattern shifts organically with time.
    vec2  noiseUV = uv * instanceUBO.noiseScale + vec2(t * 0.04, -t * 0.027);
    float noise   = FBM(noiseUV);

    // Threshold ping-pongs 0 → 1 → 0: fully visible, fully dissolved, repeat.
    float threshold = 0.5 + 0.5 * sin(t * instanceUBO.dissolveSpeed * 6.2831853);

    // Discard pixels below the threshold — they have dissolved.
    if (noise < threshold) discard;

    // ── Edge fire ────────────────────────────────────────────────────────────
    // noiseAbove: how far above the threshold this pixel sits.
    float noiseAbove = noise - threshold;
    float edgeW      = max(instanceUBO.edgeWidth, 0.001);
    float edgeFactor = clamp(noiseAbove / edgeW, 0.0, 1.0); // 0 = boundary, 1 = surface

    // 3-zone gradient: white-hot core → artist edge color → dark ember → surface.
    vec3 hotCore  = vec3(1.6, 1.5, 0.9);                    // white-hot
    vec3 fireMid  = instanceUBO.edgeColor.rgb;               // artist-defined (default orange)
    vec3 darkEdge = fireMid * 0.15;                          // dark ember
    float glowI   = instanceUBO.edgeColor.a;

    vec3 fireGrad;
    if (edgeFactor < 0.35)
        fireGrad = mix(hotCore,  fireMid,  edgeFactor / 0.35);
    else if (edgeFactor < 0.65)
        fireGrad = mix(fireMid,  darkEdge, (edgeFactor - 0.35) / 0.30);
    else
        fireGrad = mix(darkEdge, vec3(0.0), (edgeFactor - 0.65) / 0.35);

    // ── Surface lighting ─────────────────────────────────────────────────────
    vec3 albedo = texture(diffuseSampler, uv).rgb * instanceUBO.diffuseColor.rgb;

    mat3 TBN      = mat3(normalize(inDTO.T), normalize(inDTO.B), normalize(inDTO.N));
    vec3 ns       = texture(normalSampler, uv).rgb * 2.0 - 1.0;
    ns.xy        *= instanceUBO.normalStrength;
    vec3 N        = normalize(TBN * ns);
    vec3 V        = normalize(globalUBO.viewPosition.xyz - inDTO.fragPos);

    vec3 ambient  = globalUBO.ambientColor.rgb * globalUBO.ambientColor.w * albedo;
    vec3 direct   = CalcDirectionalLight(globalUBO.dirLight.direction, globalUBO.dirLight.color,
                                         N, V, albedo);
    for (int i = 0; i < globalUBO.lightCountAndPad.x; i++)
        direct += CalcPointLight(globalUBO.pointLights[i].position, globalUBO.pointLights[i].color,
                                 inDTO.fragPos, N, V, albedo);

    vec3 litSurface = ambient + direct;

    // ── Subsurface heat glow ──────────────────────────────────────────────────
    // Pixels that survive near the edge get a warm tint — the material is
    // "heating up" just before it vanishes.
    float heatReach = edgeW * 2.5;
    float heatFactor = 1.0 - smoothstep(0.0, heatReach, noiseAbove);
    litSurface += heatFactor * fireMid * glowI * 0.6;

    // ── Blend fire edge into surface ─────────────────────────────────────────
    // edgeFactor=0 (boundary) → pure fire; edgeFactor=1 (far from boundary) → pure surface.
    vec3 result = mix(fireGrad * glowI, litSurface, smoothstep(0.0, 1.0, edgeFactor));

    fragColor = vec4(result, 1.0);
}
