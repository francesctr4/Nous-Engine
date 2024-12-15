#include "TextureSystem.h"

#include "Logger.h"
#include "MemoryManager.h"

#include "ModuleRenderer3D.h"
#include "RendererFrontend.h"

bool NOUS_TextureSystem::Initialize()
{
	CreateDefaultTexture();

	return true;
}

void NOUS_TextureSystem::Shutdown()
{
	DestroyDefaultTexture();
}

Texture* NOUS_TextureSystem::AcquireTexture(const char* name, bool autoRelease)
{
	return nullptr;
}

void NOUS_TextureSystem::ReleaseTexture(const char* name)
{

}

bool NOUS_TextureSystem::CreateDefaultTexture()
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

	External->renderer->rendererFrontend->CreateTexture(DEFAULT_TEXTURE_NAME, texDimension, texDimension, 4, pixels.data(), false, &defaultTexture);

	// Manually set the texture generation to invalid since this is a default texture.
	defaultTexture.generation = INVALID_ID;

	return true;
}

Texture* NOUS_TextureSystem::GetDefaultTexture()
{
	return &defaultTexture;
}

void NOUS_TextureSystem::DestroyDefaultTexture()
{
	External->renderer->rendererFrontend->DestroyTexture(&defaultTexture);
}
