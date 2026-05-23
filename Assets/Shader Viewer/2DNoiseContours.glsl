// 2D Noise Contours by Shane
// https://www.shadertoy.com/view/XdcGzB

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
    float aspectRatio;   // width/height for coordinate centering (default 1.7778 for 16:9)
    float resolution;    // vertical pixel count — scales AA contour smoothing (default 1080.0)
    vec2  uvTiles;       // UV tile counts of the mesh — normalizes texCoord to 0..1 (default 4.0, 2.25 for Rectangle.obj)
} instanceUBO;

// ── Hash / Noise ──────────────────────────────────────────────────────────────

vec2 hash22(vec2 p)
{
    float n = sin(dot(p, vec2(41.0, 289.0)));
    p = fract(vec2(2097152.0, 262144.0) * n);
    return cos(p * 6.283 + globalUBO.time.x * 2.0);
}

float simplesque2D(vec2 p)
{
    vec2 s = floor(p + (p.x + p.y) * 0.3660254);
    p -= s - (s.x + s.y) * 0.2113249;

    float i    = p.x < p.y ? 1.0 : 0.0;
    vec2  ioffs = vec2(1.0 - i, i);

    vec2 p1 = p - ioffs + 0.2113249;
    vec2 p2 = p - 0.5773502;

    vec3 d = max(0.5 - vec3(dot(p, p), dot(p1, p1), dot(p2, p2)), 0.0);
    d *= d * d * 12.0;

    vec3 w = vec3(dot(hash22(s),          p),
                  dot(hash22(s + ioffs),   p1),
                  dot(hash22(s + 1.0),     p2));

    return 0.5 + dot(w, d);
}

float valueNoise2D(vec2 p)
{
    vec2 f = fract(p);
    f *= f * (3.0 - 2.0 * f);

    vec4 h = fract(sin(vec4(0.0, 41.0, 289.0, 330.0) + dot(floor(p), vec2(41.0, 289.0))) * 43758.5453);
    h = sin(h * 6.283 + globalUBO.time.x) * 0.5 + 0.5;

    return dot(vec2(1.0 - f.y, f.y), vec2(1.0 - f.x, f.x) * mat2(h));
}

float func2D(vec2 p)
{
    return simplesque2D(p * 4.0) * 0.66 + simplesque2D(p * 8.0) * 0.34;
}

// Smooth fract — produces antialiased contour lines.
// sf is the function value divided by the length of its gradient (IQ's distance estimation).
float smoothFract(float x, float sf)
{
    x = fract(x);
    return min(x, x * (1.0 - x) * sf);
}

// ── Main ──────────────────────────────────────────────────────────────────────

void main()
{
    // Normalize texCoord (0..uvTiles) → (0..1), then center and apply aspect ratio.
    vec2 uv = (inDTO.texCoord / instanceUBO.uvTiles - 0.5) * vec2(instanceUBO.aspectRatio, 1.0);

    vec2  e = vec2(0.001, 0.0);

    float f = func2D(uv);

    // Numerical gradient length — two extra function calls.
    float g = length(vec2(f - func2D(uv - e.xy),
                          f - func2D(uv - e.yx))) / e.x;

    // Gradient-normalised value for AA smoothing.
    g = f / max(g, 0.001);

    // Antialiased contour fract (6 bands).
    // instanceUBO.resolution replaces iResolution.y — scale it to match your render target.
    float c = smoothFract(f * 6.0, g * instanceUBO.resolution / 4.0);

    vec3 col = vec3(c);

    f *= 6.0;

    float tx = smoothstep(0.0, 0.8, func2D((uv + (1.0 - c) * 0.01) * vec2(12.0, 48.0)));
    if      (f > 2.0 && f < 3.0) col *= vec3(1.0, 0.0,  0.02) * tx;   // Red
    else if (f > 4.0 && f < 5.0) col *= vec3(0.15, 0.05, 1.0) * tx;   // Blue
    else                          col *= abs(tx - 0.5) * 0.4 + 0.8;    // White

    // Subtle highlight from the gradient.
    col += min(g * g * g * g * 0.1, 1.0) * (1.0 - col);

    col *= instanceUBO.diffuseColor.rgb;

    fragColor = vec4(sqrt(clamp(col, 0.0, 1.0)), instanceUBO.diffuseColor.a);
}
