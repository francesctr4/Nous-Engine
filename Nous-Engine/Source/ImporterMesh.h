#pragma once

#include "RendererTypes.inl" // For Mesh Class

//Temp (for vertex)
#include "VulkanShaderUtils.h"
//Temp

struct Mesh
{
	std::vector<Vertex> vertices;
	std::vector<uint32> indices;
};

namespace ImporterMesh 
{
	bool Import(const char* path, Mesh* mesh);
	
	//void Save();
	//void Load();
}