#pragma stage vertex
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;

layout(set = 0, binding = 0) uniform globalUniformObject
{
    mat4 projection;
    mat4 view;
    vec4 outlineColor;
} globalUBO;

layout(push_constant) uniform pushConstantObject
{
// Only guaranteed a total of 128 bytes.
    mat4 model; // 64 bytes
    float outlineThickness; // 4 bytes
} pushConstant;

void main()
{
    vec3 worldPos = vec3(pushConstant.model * vec4(inPosition, 1.0));

    mat3 normalMatrix = transpose(inverse(mat3(pushConstant.model)));
    vec3 worldNormal = normalize(normalMatrix * inNormal);

    // Scale thickness by the model's average axis scale so the outline width
    // is proportional to the mesh's world-space size. Without this, a mesh
    // scaled to 0.01 would show the same world-space extrusion as a mesh
    // at scale 1.0, making the outline look enormous relative to the mesh.
    float scaleX = length(vec3(pushConstant.model[0]));
    float scaleY = length(vec3(pushConstant.model[1]));
    float scaleZ = length(vec3(pushConstant.model[2]));
    float modelScale = (scaleX + scaleY + scaleZ) / 3.0;

    worldPos += worldNormal * pushConstant.outlineThickness * modelScale;

    gl_Position = globalUBO.projection * globalUBO.view * vec4(worldPos, 1.0);
}

// ------------------------------------------------------------------------------------------------------

#pragma stage fragment
#version 450

layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 0) uniform globalUniformObject
{
    mat4 projection;
    mat4 view;
    vec4 outlineColor;
} globalUBO;

void main()
{
    fragColor = globalUBO.outlineColor;
}
