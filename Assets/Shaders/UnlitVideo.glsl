#pragma stage vertex
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;

layout(location = 0) out struct DataTransferObject
{
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
    outDTO.texCoord = inTexCoord;
    gl_Position = globalUBO.projection * globalUBO.view * instanceData.models[gl_InstanceIndex] * vec4(inPosition, 1.0);
}

// ------------------------------------------------------------------------------------------------------

#pragma stage fragment
#version 450

// Unlit textured surface. Outputs the diffuse texture at full brightness with NO scene lighting,
// so a video/image shows its true source colors instead of being washed out by ambient + diffuse
// (which is what the lit ForwardBlinnPhong shader does). Designed for the CVideoPlayer plane:
// the video binds into `diffuseSampler` (the component's default target slot).
//
// `saturation` and `contrast` are OPTIONAL grading knobs used as direct multipliers, so 1.0 = no
// change (matching the engine's default uniform value of 1.0 → a faithful passthrough out of the box).
// Nudge them above 1.0 (e.g. 1.1 - 1.3) for extra "pop"; below 1.0 to mute. Keep at 1.0 to match 1:1.

layout(location = 0) in struct DataTransferObject
{
    vec2 texCoord;
} inDTO;

layout(location = 0) out vec4 fragColor;

layout(set = 1, binding = 0) uniform InstanceUBO
{
    vec4  diffuseColor;  // rgb = tint (white = untouched), a = unused
    float saturation;    // multiplier: 1.0 = unchanged, >1 = more saturated
    float contrast;      // multiplier: 1.0 = unchanged, >1 = more contrast
} instanceUBO;

layout(set = 1, binding = 1) uniform sampler2D diffuseSampler;

void main()
{
    vec3 color = texture(diffuseSampler, inDTO.texCoord).rgb * instanceUBO.diffuseColor.rgb;

    // Saturation around perceptual luminance (Rec. 709).
    float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(vec3(luma), color, instanceUBO.saturation);

    // Contrast around mid-gray.
    color = (color - 0.5) * instanceUBO.contrast + 0.5;

    fragColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}
