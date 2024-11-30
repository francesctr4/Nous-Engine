#version 450

#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out int outMode;

// Data Transfer Object
layout(location = 1) out struct DataTransferObject
{
	vec3 outColor;
	vec2 texCoord;
} outDTO;

layout(set = 0, binding = 0) uniform globalUniformObject 
{
    mat4 projection;
	mat4 view;
} globalUBO;

layout(push_constant) uniform pushConstantObject
{
	// Only guaranteed a total of 128 bytes.
	mat4 model; // 64 bytes
} pushConstant;

void main() 
{
	outDTO.outColor = inColor;
	outDTO.texCoord = inTexCoord;

    gl_Position = globalUBO.projection * globalUBO.view * pushConstant.model * vec4(inPosition, 1.0);
}