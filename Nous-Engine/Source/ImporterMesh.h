#pragma once

#include "RendererTypes.inl" // For Mesh Class

namespace ImporterMesh 
{
	bool Import(const char* path, Mesh* mesh);
	bool Save(const char* path, const Mesh& mesh);
	bool Load(const char* path, Mesh* mesh);
}