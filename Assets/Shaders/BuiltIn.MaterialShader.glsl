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

// Data Transfer Object
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
    vec4 diffuseColor;     // rgb = tint, a = opacity (unused)
} instanceUBO;

// Samplers
layout(set = 1, binding = 1) uniform sampler2D diffuseSampler;

void main()
{
    vec4 diffuse = instanceUBO.diffuseColor * texture(diffuseSampler, inDTO.texCoord);
    fragColor = diffuse;
}
