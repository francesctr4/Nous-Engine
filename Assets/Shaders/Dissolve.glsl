#pragma stage vertex
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;
// location 4 = smoothNormal (outline only, not used here)
layout(location = 5) in vec4 inTangent;
layout(location = 7) in uvec4 inBoneIDs;
layout(location = 8) in vec4  inBoneWeights;

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
        float diff      = max(dot(N, L), 0.0);
        vec3  H         = normalize(L + V);
        float spec      = pow(max(dot(N, H), 0.0), 32.0) * 0.3;
        direct += (diff * albedo + spec) * sl.color.rgb * sl.color.w * att * spotFade;
    }

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
