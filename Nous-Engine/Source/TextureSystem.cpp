#include "TextureSystem.h"

#include "Logger.h"
#include "MemoryManager.h"

#include "ModuleRenderer3D.h"
#include "RendererFrontend.h"

bool NOUS_TextureSystem::Initialize()
{
	CreateDefaultTextures();

	return true;
}

void NOUS_TextureSystem::Shutdown()
{
	DestroyDefaultTextures();
}

ResourceTexture* NOUS_TextureSystem::AcquireTexture(const char* name, bool autoRelease)
{
	// Return default texture, but warn about it since this should be returned via get_default_texture();
	//if (strings_equali(name, DEFAULT_TEXTURE_NAME)) {
	//	KWARN("texture_system_acquire called for default texture. Use texture_system_get_default_texture for texture 'default'.");
	//	return &state_ptr->default_texture;
	//}
	//texture_reference ref;
	//if (state_ptr && hashtable_get(&state_ptr->registered_texture_table, name, &ref)) {
	//	// This can only be changed the first time a texture is loaded.
	//	if (ref.reference_count == 0) {
	//		ref.auto_release = auto_release;
	//	}
	//	ref.reference_count++;
	//	if (ref.handle == INVALID_ID) {
	//		// This means no texture exists here. Find a free index first.
	//		u32 count = state_ptr->config.max_texture_count;
	//		texture* t = 0;
	//		for (u32 i = 0; i < count; ++i) {
	//			if (state_ptr->registered_textures[i].id == INVALID_ID) {
	//				// A free slot has been found. Use its index as the handle.
	//				ref.handle = i;
	//				t = &state_ptr->registered_textures[i];
	//				break;
	//			}
	//		}
	//		// Make sure an empty slot was actually found.
	//		if (!t || ref.handle == INVALID_ID) {
	//			KFATAL("texture_system_acquire - Texture system cannot hold anymore textures. Adjust configuration to allow more.");
	//			return 0;
	//		}
	//		// Create new texture.
	//		if (!load_texture(name, t)) {
	//			KERROR("Failed to load texture '%s'.", name);
	//			return 0;
	//		}
	//		// Also use the handle as the texture id.
	//		t->id = ref.handle;
	//		KTRACE("Texture '%s' does not yet exist. Created, and ref_count is now %i.", name, ref.reference_count);
	//	}
	//	else {
	//		KTRACE("Texture '%s' already exists, ref_count increased to %i.", name, ref.reference_count);
	//	}
	//	// Update the entry.
	//	hashtable_set(&state_ptr->registered_texture_table, name, &ref);
	//	return &state_ptr->registered_textures[ref.handle];
	//}
	//// NOTE: This would only happen in the event something went wrong with the state.
	//KERROR("texture_system_acquire failed to acquire texture '%s'. Null pointer will be returned.", name);
	return 0;
}

void NOUS_TextureSystem::ReleaseTexture(ResourceTexture* texture)
{
	External->renderer->rendererFrontend->DestroyTexture(texture);

	// Clean up backend resources.
	//texture->name.clear();
	MemoryManager::ZeroMemory(texture, sizeof(ResourceTexture));

	texture->ID = INVALID_ID;
	texture->generation = INVALID_ID;
}

bool NOUS_TextureSystem::CreateDefaultTextures()
{
	// NOTE: Create default texture, a 256x256 blue/white checkerboard pattern.
	// This is done in code to eliminate asset dependencies.

	NOUS_TRACE("Creating default texture...");

	const uint32 texDimension = 256;
	const uint32 channels = 4;
	const uint32 pixelCount = texDimension * texDimension;
	const uint32 squareSize = 16; // Size of each checkerboard square in pixels.

	std::array<uint8, (pixelCount* channels)> pixels;
	MemoryManager::SetMemory(pixels.data(), 255, sizeof(uint8) * pixelCount * channels);

	// Create the checkerboard pattern with larger squares.
	for (uint64 row = 0; row < texDimension; ++row)
	{
		for (uint64 col = 0; col < texDimension; ++col)
		{
			uint64 index = (row * texDimension) + col;
			uint64 indexBpp = index * channels;

			// Determine which square we are in.
			bool isWhiteSquare = ((row / squareSize) % 2 == (col / squareSize) % 2);

			if (isWhiteSquare)
			{
				// White color.
				pixels[indexBpp + 0] = 255; // Red
				pixels[indexBpp + 1] = 255; // Green
				pixels[indexBpp + 2] = 255; // Blue
				pixels[indexBpp + 3] = 255; // Alpha
			}
			else
			{
				// Blue color.
				pixels[indexBpp + 0] = 0;   // Red
				pixels[indexBpp + 1] = 0;   // Green
				pixels[indexBpp + 2] = 255; // Blue
				pixels[indexBpp + 3] = 255; // Alpha
			}
		}
	}

	state.defaultTexture.SetName(state.config.DEFAULT_TEXTURE_NAME);
	state.defaultTexture.width = texDimension;
	state.defaultTexture.height = texDimension;
	state.defaultTexture.channelCount = 4;

	External->renderer->rendererFrontend->CreateTexture(pixels.data(), &state.defaultTexture);

	// Manually set the texture generation to invalid since this is a default texture.
	state.defaultTexture.generation = INVALID_ID;

	return true;
}

ResourceTexture* NOUS_TextureSystem::GetDefaultTexture()
{
	return &state.defaultTexture;
}

void NOUS_TextureSystem::DestroyDefaultTextures()
{
	External->renderer->rendererFrontend->DestroyTexture(&state.defaultTexture);
}
