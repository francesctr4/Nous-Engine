#version 450

layout(location = 0) in flat int outMode;

// Data Transfer Object
layout(location = 1) in struct DataTransferObject
{
	vec3 outColor;
	vec2 texCoord;
} inDTO;

layout(location = 0) out vec4 fragColor;

layout(set = 1, binding = 0) uniform localUniformObject 
{
    vec4 diffuseColor;
} localUBO;

// Samplers
layout(set = 1, binding = 1) uniform sampler2D diffuseSampler;

void main() 
{
    fragColor = /* vec4(inDTO.outColor, 1.0f) * */ localUBO.diffuseColor * texture(diffuseSampler, inDTO.texCoord);
}