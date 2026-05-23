// Recursive Noise Experiment by ompuco
// https://www.shadertoy.com/view/wllGzr

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
    vec4 diffuseColor;  // rgb = color tint, a = opacity
    vec2 uvOffset;      // replaces iMouse.xy/iResolution.xy/4 — pan the noise (default 0,0)
} instanceUBO;

// ── Noise ─────────────────────────────────────────────────────────────────────

float hash(float n)
{
    return fract(sin(n) * 43758.5453);
}

// Returns a value in the range -0.5 .. 0.5
float noise(vec3 x)
{
    vec3  p = floor(x);
    vec3  f = fract(x);
    f       = f * f * (3.0 - 2.0 * f);
    float n = p.x + p.y * 57.0 + 113.0 * p.z;

    return mix(mix(mix(hash(n +   0.0), hash(n +   1.0), f.x),
                   mix(hash(n +  57.0), hash(n +  58.0), f.x), f.y),
               mix(mix(hash(n + 113.0), hash(n + 114.0), f.x),
                   mix(hash(n + 170.0), hash(n + 171.0), f.x), f.y), f.z) - 0.5;
}

// ── Main ──────────────────────────────────────────────────────────────────────

void main()
{
    // iFrame * vec3(1,2,3) / 1000 ≈ time * 60fps * vec3(1,2,3) / 1000
    vec3 t = globalUBO.time.x * vec3(1.0, 2.0, 3.0) * 0.06;

    // texCoord (0..1) replaces fragCoord/iResolution.  The original then zooms in
    // to the centre with uv/4+0.5; uvOffset replaces the iMouse pan term.
    vec2 uv = inDTO.texCoord / 4.0 + 0.5;
    uv -= instanceUBO.uvOffset;

    vec3 col = vec3(0.0);

    for (int i = 0; i < 16; i++)
    {
        float i2 = float(i);
        vec3  sgn = vec3(sign(sin(i2 / 3.0)));
        col.r += noise(uv.xyy * (12.0 + i2) + col.rgb + t * sgn);
        col.g += noise(uv.xyx * (12.0 + i2) + col.rgb + t * sgn);
        col.b += noise(uv.yyx * (12.0 + i2) + col.rgb + t * sgn);
    }

    for (int i = 0; i < 16; i++)
    {
        float i2 = float(i);
        vec3  sgn = vec3(sign(sin(i2 / 3.0)));
        col.r += noise(uv.xyy * 32.0 + col.rgb + t * sgn);
        col.g += noise(uv.xyx * 32.0 + col.rgb + t * sgn);
        col.b += noise(uv.yyx * 32.0 + col.rgb + t * sgn);
    }

    col.rgb /= 32.0;
    col.rgb  = mix(col.rgb, normalize(col.rgb) * 2.0, 1.0);
    col.rgb += 0.3;

    col.rgb *= instanceUBO.diffuseColor.rgb;

    fragColor = vec4(col, instanceUBO.diffuseColor.a);
}
