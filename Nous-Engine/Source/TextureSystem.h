#pragma once

#include "RendererTypes.inl"

namespace NOUS_TextureSystem 
{
	bool Initialize();
	void Shutdown();

	Texture* AcquireTexture(const char* name, bool autoRelease);
	void ReleaseTexture(Texture* texture);

	// ------------------------------------------------------------------------------------------------------ //

	bool CreateDefaultTextures();
	Texture* GetDefaultTexture();
	void DestroyDefaultTextures();

	// ------------------------------------------------------------------------------------------------------ //

	struct TextureSystemConfig 
	{
		const char* DEFAULT_TEXTURE_NAME = "DefaultTexture";
		const uint32 MAX_TEXTURE_COUNT = 65536;
	};

	struct TextureReference 
	{
		uint64 referenceCount;
		Texture texture;
		bool autoRelease;
	};

	struct TextureSystemState 
	{
		TextureSystemConfig config;
		Texture defaultTexture;
		// Array of registered textures.
		Texture* registeredTextures;
		// Hashtable for texture lookups.
		//hashtable registered_texture_table;
	};

	static TextureSystemState state;

}