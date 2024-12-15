#pragma once

#include "RendererTypes.inl" // For Texture Class

namespace ImporterTexture 
{
	bool Import(const char* path, Texture* texture);
	//bool Save(const char* path, const Texture& texture);
	//bool Load(const char* path, Texture* texture);
}