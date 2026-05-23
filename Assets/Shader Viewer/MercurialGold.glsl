// Mercurial Gold by morphix
// https://www.shadertoy.com/view/3tyBRW

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
    vec4  diffuseColor;  // rgb = color scale (default 4.3, 3.4, 0.1), a = opacity (default 0.1)
    float aspectRatio;   // width/height of the surface — corrects coordinate centering (default 1.7778)
    float colorBias;     // subtracted from final rgb before output (default 0.35)
    vec2  uvTiles;       // UV tile counts of the mesh — normalizes texCoord to 0..1 (default 4.0, 2.25 for Rectangle.obj)
} instanceUBO;

// ── Main ──────────────────────────────────────────────────────────────────────

void main()
{
    float iTime = globalUBO.time.x;

    // Normalize texCoord (0..uvTiles) → (0..1), then center and apply aspect ratio.
    vec2 uv = inDTO.texCoord / instanceUBO.uvTiles;
    vec2 p  = 5.0 * vec2((uv.x - 0.5) * instanceUBO.aspectRatio, uv.y - 0.5) - 0.5;
    vec2 i  = p;

    float c   = 0.0;
    float r   = length(p + vec2(sin(iTime), sin(iTime * 0.222 + 99.0)) * 1.5);
    float d   = length(p);
    float rot = d + iTime + p.x * 0.15;

    for (float n = 0.0; n < 4.0; n++)
    {
        p *= mat2( cos(rot - sin(iTime / 4.0)),  sin(rot),
                  -sin(cos(rot) - iTime),         cos(rot)) * -0.15;
        float t = r - iTime / (n + 1.5);
        i -= p + vec2(cos(t - i.x - r) + sin(t + i.y),
                      sin(t - i.y) + cos(t + i.x) + r);
        c += 1.0 / length(vec2(sin(i.x + t) / 0.15, cos(i.y + t) / 0.15));
    }
    c /= 4.0;

    vec3 colour = vec3(c) * instanceUBO.diffuseColor.rgb - instanceUBO.colorBias;

    fragColor = vec4(colour, instanceUBO.diffuseColor.a);
}
