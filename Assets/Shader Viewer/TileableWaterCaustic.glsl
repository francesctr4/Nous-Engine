// Tileable Water Caustic by Dave_Hoskins
// https://www.shadertoy.com/view/MdlXz8

#pragma stage vertex
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;

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

void main()
{
    outDTO.outColor = inColor;
    outDTO.texCoord = inTexCoord;

    gl_Position = globalUBO.projection * globalUBO.view * instanceData.models[gl_InstanceIndex] * vec4(inPosition, 1.0);
}

// ------------------------------------------------------------------------------------------------------

#pragma stage fragment
#version 450

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
    vec4  diffuseColor;  // rgb = color tint, a = opacity
    float speed;         // animation speed multiplier (default 0.5)
    float tileScale;     // UV tiling — 1.0 = standard, 2.0 = 2×2 tiles (default 1.0)
    float intensity;     // wave sharpness — inten in original (default 0.005)
    float _pad;
} instanceUBO;

// ── Main ──────────────────────────────────────────────────────────────────────

void main()
{
    const float TAU      = 6.28318530718;
    const int   MAX_ITER = 5;

    // iTime * 0.5 + 23.0 in the original; speed replaces the 0.5 multiplier.
    float time = globalUBO.time.x * instanceUBO.speed + 23.0;

    // texCoord (0..1) replaces fragCoord/iResolution.
    // tileScale=1 → original non-tiled look; tileScale=2 → 2×2 tiled look.
    vec2 uv = inDTO.texCoord;
    vec2 p  = mod(uv * TAU * instanceUBO.tileScale, TAU) - 250.0;
    vec2 i  = vec2(p);

    float c     = 1.0;
    float inten = instanceUBO.intensity;

    for (int n = 0; n < MAX_ITER; n++)
    {
        float t = time * (1.0 - (3.5 / float(n + 1)));
        i = p + vec2(cos(t - i.x) + sin(t + i.y),
                     sin(t - i.y) + cos(t + i.x));
        c += 1.0 / length(vec2(p.x / (sin(i.x + t) / inten),
                               p.y / (cos(i.y + t) / inten)));
    }

    c /= float(MAX_ITER);
    c  = 1.17 - pow(c, 1.4);

    vec3 colour = vec3(pow(abs(c), 8.0));
    colour = clamp(colour + vec3(0.0, 0.35, 0.5), 0.0, 1.0);
    colour *= instanceUBO.diffuseColor.rgb;

    fragColor = vec4(colour, instanceUBO.diffuseColor.a);
}
