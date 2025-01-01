#pragma once

#include "RendererTypes.inl" // For Mesh Class
#include "MaterialSystem.h"

namespace ImporterMaterial
{
	bool Load(const char* path, NOUS_MaterialSystem::MaterialConfig* materialConfig);
	bool Save(const char* path, const Material& material);
}