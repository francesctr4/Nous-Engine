#pragma once

#include "RendererTypes.inl"

namespace NOUS_TextureSystem 
{
	bool Initialize();
	void Shutdown();

	Texture* AcquireTexture(const char* name, bool autoRelease);
	void ReleaseTexture(const char* name);

	// ------------------------------------------------------------------------------------------------------ //

	bool CreateDefaultTexture();
	Texture* GetDefaultTexture();
	void DestroyDefaultTexture();

	// ------------------------------------------------------------------------------------------------------ //

	static Texture defaultTexture;

	constexpr const char* DEFAULT_TEXTURE_NAME = "DefaultTexture";
	constexpr uint32 MAX_TEXTURE_COUNT = 65536;
}