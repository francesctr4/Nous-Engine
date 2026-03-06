#pragma stage vertex
#version 450

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

layout(location = 0) out vec2 vUV;

layout(set = 0, binding = 0) uniform CameraUBO
{
    mat4 uViewProj;
} camera;

layout(push_constant) uniform Push
{
    mat4 uModel;
    vec4 uTint;
} pc;

void main()
{
    vUV = aUV;
    gl_Position = camera.uViewProj * pc.uModel * vec4(aPos, 1.0);
}

// -------------------------------------------------------------------

#pragma stage fragment
#version 450

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D uAlbedo;

layout(push_constant) uniform Push
{
    mat4 uModel;
    vec4 uTint;
} pc;

void main()
{
    outColor = texture(uAlbedo, vUV) * pc.uTint;
}