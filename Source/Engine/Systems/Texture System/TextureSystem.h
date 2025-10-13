#ifndef TEXTURESYSTEM_H
#define TEXTURESYSTEM_H

#include "Engine/Renderer/RendererTypes.h"
#include "Engine/Systems/Resource Manager/Resource Types/ResourceTexture.h"
#include "Engine/Core/Globals.h"

namespace NOUS_TextureSystem 
{
	bool Initialize();
	void Shutdown();

	ResourceTexture* AcquireTexture(const char* name, bool autoRelease);
	void ReleaseTexture(ResourceTexture* texture);

	// ------------------------------------------------------------------------------------------------------ //

	bool CreateDefaultTextures();
	ResourceTexture* GetDefaultTexture();
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
		ResourceTexture texture;
		bool autoRelease;
	};

	struct TextureSystemState 
	{
		TextureSystemConfig config;
		ResourceTexture defaultTexture;
		// Array of registered textures.
		ResourceTexture* registeredTextures;
		// Hashtable for texture lookups.
		//hashtable registered_texture_table;
	};

	static TextureSystemState state;

}

#endif // TEXTURESYSTEM_H