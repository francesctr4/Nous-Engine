#pragma once

#include "RendererTypes.inl" // For Texture Class

struct Texture;

namespace ImporterTexture 
{
	void Import(const char* path, Texture* texture);

	//void Save();
	//void Load();
}