// All-stages template: vertex → tessControl → tessEvaluation → geometry → fragment.
// The tess and geometry stages are identity pass-throughs (tess level 1, no subdivision).
// Lighting is identical to ForwardBlinnPhong. InstanceUBO layout is cross-compatible.
//
// Vulkan GLSL rules for multi-stage shaders:
//   - gl_PerVertex must be explicitly redeclared in every stage that references gl_Position.
//   - All stages must declare the SAME members in gl_PerVertex to satisfy interface matching.
//   - User-defined inter-stage data in tess/geom stages must use NAMED interface blocks
//     (bare per-vertex arrays are unreliable across shaderc versions in Vulkan mode).
//   - Named blocks are matched between stages by BLOCK TYPE NAME (not instance name).

// ============================================================
// VERTEX
// ============================================================
#pragma stage vertex
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;
// location 4 = smoothNormal (outline only, not used here)
layout(location = 5) in vec4 inTangent;

// Redeclare gl_PerVertex with only gl_Position so all downstream stages can match it exactly.
out gl_PerVertex { vec4 gl_Position; };

layout(location = 0) out VS_Out {
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

// ============================================================
// TESS CONTROL — pass-through, tess level 1 (no subdivision)
// Block name VS_Out matches the vertex output block.
// Block name TC_Out is consumed by tessEvaluation.
// ============================================================
#pragma stage tessControl
#version 450

layout(vertices = 3) out;

in  gl_PerVertex { vec4 gl_Position; } gl_in[];
out gl_PerVertex { vec4 gl_Position; } gl_out[];

layout(location = 0) in VS_Out {
    vec3 outColor;
    vec2 texCoord;
    vec3 fragPos;
    vec3 T;
    vec3 B;
    vec3 N;
} tc_in[];

layout(location = 0) out TC_Out {
    vec3 outColor;
    vec2 texCoord;
    vec3 fragPos;
    vec3 T;
    vec3 B;
    vec3 N;
} tc_out[];

void main()
{
    tc_out[gl_InvocationID].outColor = tc_in[gl_InvocationID].outColor;
    tc_out[gl_InvocationID].texCoord = tc_in[gl_InvocationID].texCoord;
    tc_out[gl_InvocationID].fragPos  = tc_in[gl_InvocationID].fragPos;
    tc_out[gl_InvocationID].T        = tc_in[gl_InvocationID].T;
    tc_out[gl_InvocationID].B        = tc_in[gl_InvocationID].B;
    tc_out[gl_InvocationID].N        = tc_in[gl_InvocationID].N;

    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;

    if (gl_InvocationID == 0)
    {
        gl_TessLevelOuter[0] = 1.0;
        gl_TessLevelOuter[1] = 1.0;
        gl_TessLevelOuter[2] = 1.0;
        gl_TessLevelInner[0] = 1.0;
    }
}

// ============================================================
// TESS EVALUATION — barycentric interpolation (identity at level 1)
// Block name TC_Out matches the tessControl output block.
// Block name TE_Out is consumed by geometry.
// ============================================================
#pragma stage tessEvaluation
#version 450

layout(triangles, equal_spacing, cw) in;

in  gl_PerVertex { vec4 gl_Position; } gl_in[];
out gl_PerVertex { vec4 gl_Position; };

layout(location = 0) in TC_Out {
    vec3 outColor;
    vec2 texCoord;
    vec3 fragPos;
    vec3 T;
    vec3 B;
    vec3 N;
} te_in[];

layout(location = 0) out TE_Out {
    vec3 outColor;
    vec2 texCoord;
    vec3 fragPos;
    vec3 T;
    vec3 B;
    vec3 N;
} te_out;

void main()
{
    vec3 u = gl_TessCoord;

    te_out.outColor = u.x * te_in[0].outColor + u.y * te_in[1].outColor + u.z * te_in[2].outColor;
    te_out.texCoord = u.x * te_in[0].texCoord + u.y * te_in[1].texCoord + u.z * te_in[2].texCoord;
    te_out.fragPos  = u.x * te_in[0].fragPos  + u.y * te_in[1].fragPos  + u.z * te_in[2].fragPos;
    te_out.T        = u.x * te_in[0].T        + u.y * te_in[1].T        + u.z * te_in[2].T;
    te_out.B        = u.x * te_in[0].B        + u.y * te_in[1].B        + u.z * te_in[2].B;
    te_out.N        = u.x * te_in[0].N        + u.y * te_in[1].N        + u.z * te_in[2].N;

    gl_Position = u.x * gl_in[0].gl_Position
                + u.y * gl_in[1].gl_Position
                + u.z * gl_in[2].gl_Position;
}

// ============================================================
// GEOMETRY — triangle pass-through
// Block name TE_Out matches the tessEvaluation output block.
// Block name GE_Out is consumed by fragment.
// ============================================================
#pragma stage geometry
#version 450

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

in  gl_PerVertex { vec4 gl_Position; } gl_in[];

layout(location = 0) in TE_Out {
    vec3 outColor;
    vec2 texCoord;
    vec3 fragPos;
    vec3 T;
    vec3 B;
    vec3 N;
} ge_in[];

layout(location = 0) out GE_Out {
    vec3 outColor;
    vec2 texCoord;
    vec3 fragPos;
    vec3 T;
    vec3 B;
    vec3 N;
} ge_out;

void main()
{
    for (int i = 0; i < 3; ++i)
    {
        ge_out.outColor = ge_in[i].outColor;
        ge_out.texCoord = ge_in[i].texCoord;
        ge_out.fragPos  = ge_in[i].fragPos;
        ge_out.T        = ge_in[i].T;
        ge_out.B        = ge_in[i].B;
        ge_out.N        = ge_in[i].N;
        gl_Position     = gl_in[i].gl_Position;
        EmitVertex();
    }
    EndPrimitive();
}

// ============================================================
// FRAGMENT — identical lighting to ForwardBlinnPhong
// Block name GE_Out matches the geometry output block.
// ============================================================
#pragma stage fragment
#version 450

layout(location = 0) in GE_Out {
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

vec3 CalcDirectionalLight(vec4 dirAndUnused, vec4 colorAndIntensity,
                          vec3 normal, vec3 viewDir,
                          vec3 albedo, float specStrength, float shininess)
{
    vec3  lightDir = normalize(-dirAndUnused.xyz);
    float diff     = max(dot(normal, lightDir), 0.0);
    vec3  halfway  = normalize(lightDir + viewDir);
    float spec     = pow(max(dot(normal, halfway), 0.0), shininess);
    return diff * albedo * colorAndIntensity.rgb * colorAndIntensity.w
         + spec * specStrength * colorAndIntensity.rgb * colorAndIntensity.w;
}

vec3 CalcPointLight(vec4 posAndRange, vec4 colorAndIntensity,
                    vec3 fragPos, vec3 normal, vec3 viewDir,
                    vec3 albedo, float specStrength, float shininess)
{
    vec3  toLight = posAndRange.xyz - fragPos;
    float dist    = length(toLight);
    if (dist >= posAndRange.w) return vec3(0.0);

    float atten   = clamp(1.0 - (dist / posAndRange.w), 0.0, 1.0);
    atten        *= atten;

    vec3  lightDir = normalize(toLight);
    float diff     = max(dot(normal, lightDir), 0.0);
    vec3  halfway  = normalize(lightDir + viewDir);
    float spec     = pow(max(dot(normal, halfway), 0.0), shininess);

    return (diff * albedo * colorAndIntensity.rgb * colorAndIntensity.w
          + spec * specStrength * colorAndIntensity.rgb * colorAndIntensity.w) * atten;
}

vec3 CalcSpotLight(SpotLight light, vec3 fragPos, vec3 normal, vec3 viewDir,
                   vec3 albedo, float specStrength, float shininess)
{
    vec3  toLight  = light.position.xyz - fragPos;
    float dist     = length(toLight);
    if (dist >= light.position.w) return vec3(0.0);

    vec3  lightDir = normalize(toLight);
    float cosTheta = dot(lightDir, normalize(-light.direction.xyz));

    float epsilon   = light.angles.x - light.angles.y;
    float spotAtten = clamp((cosTheta - light.angles.y) / epsilon, 0.0, 1.0);

    float distAtten = clamp(1.0 - (dist / light.position.w), 0.0, 1.0);
    distAtten      *= distAtten;

    vec3  halfway  = normalize(lightDir + viewDir);
    float diff     = max(dot(normal, lightDir), 0.0);
    float spec     = pow(max(dot(normal, halfway), 0.0), shininess);

    vec3 diffuse  = diff * albedo * light.color.rgb * light.color.w;
    vec3 specular = spec * specStrength * light.color.rgb * light.color.w;
    return (diffuse + specular) * spotAtten * distAtten;
}

void main()
{
    vec2 uv = inDTO.texCoord;

    vec3  albedo       = texture(diffuseSampler,    uv).rgb * instanceUBO.diffuseColor.rgb;
    float specStrength = texture(specularSampler,   uv).r   * instanceUBO.specularIntensity;
    float shininess    = max(texture(shininessSampler, uv).r * 256.0 * instanceUBO.shininessScale, 1.0);
    float ao           = mix(1.0, texture(aoSampler, uv).r,  instanceUBO.aoIntensity);
    vec3  emissive     = texture(emissiveSampler,   uv).rgb  * instanceUBO.emissiveColor.rgb * instanceUBO.emissiveColor.a;

    mat3 TBN = mat3(normalize(inDTO.T), normalize(inDTO.B), normalize(inDTO.N));
    vec3 normalSample = texture(normalSampler, uv).rgb * 2.0 - 1.0;
    normalSample.xy  *= instanceUBO.normalStrength;
    vec3 normal       = normalize(TBN * normalSample);

    vec3 viewDir = normalize(globalUBO.viewPosition.xyz - inDTO.fragPos);

    vec3 result = globalUBO.ambientColor.rgb * globalUBO.ambientColor.w * albedo * ao;

    result += CalcDirectionalLight(globalUBO.dirLight.direction, globalUBO.dirLight.color,
                                   normal, viewDir, albedo, specStrength, shininess);

    for (int i = 0; i < globalUBO.lightCountAndPad.x; i++)
        result += CalcPointLight(globalUBO.pointLights[i].position, globalUBO.pointLights[i].color,
                                 inDTO.fragPos, normal, viewDir, albedo, specStrength, shininess);

    // --- SPOT LIGHTS ---
    for (int i = 0; i < globalUBO.spotLightCountAndPad.x; i++)
    {
        result += CalcSpotLight(globalUBO.spotLights[i],
                                inDTO.fragPos, normal, viewDir,
                                albedo, specStrength, shininess);
    }

    result += emissive;
    fragColor = vec4(result, 1.0);
}

