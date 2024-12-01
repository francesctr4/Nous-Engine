#include "RendererFrontend.h"
#include "RendererBackend.h"

#include "MemoryManager.h"
#include "Logger.h"

RendererBackend* RendererFrontend::backend = nullptr;

RendererFrontend::RendererFrontend()
{
	backend = NOUS_NEW<RendererBackend>(MemoryManager::MemoryTag::RENDERER);
}

RendererFrontend::~RendererFrontend()
{
	NOUS_DELETE(backend, MemoryManager::MemoryTag::RENDERER);
}

bool RendererFrontend::Initialize()
{
	bool ret = true;

	// Take a pointer to default textures for use in the backend.
	backend->diffuseTexture = &defaultTexture;

	// TODO: Make this configurable
	backend->Create(RendererBackendType::VULKAN);
	backend->frameNumber = 0;

	if (!backend->Initalize()) 
	{
		NOUS_FATAL("Renderer backend failed to initialize. Shutting down.");
		ret = false;
	}

	CreateDefaultTexture();

	// TODO: load other textures
	MemoryManager::ZeroMemory(&testDiffuse, sizeof(Texture));
	testDiffuse.generation = INVALID_ID;

	return ret;
}

void RendererFrontend::Shutdown()
{
	DestroyTexture(&defaultTexture);
	DestroyTexture(&testDiffuse);

	backend->Shutdown();
}

void RendererFrontend::OnResized(uint16 width, uint16 height)
{
	backend->Resized(width, height);
}

bool RendererFrontend::BeginFrame(float dt)
{
	return backend->BeginFrame(dt);
}

bool RendererFrontend::EndFrame(float dt)
{
	bool result = backend->EndFrame(dt);
	backend->frameNumber++;

	return result;
}

void RendererFrontend::UpdateGlobalState(float4x4 projection, float4x4 view, float3 viewPosition, float4 ambientColor, int32 mode)
{
	backend->UpdateGlobalState(projection, view, viewPosition, ambientColor, mode);
}

void RendererFrontend::UpdateObject(GeometryRenderData renderData)
{
	backend->UpdateObject(renderData);
}

void RendererFrontend::CreateTexture(const char* path, bool autoRelease, int32 width, int32 height, 
	int32 channelCount, const uint8* pixels, bool hasTransparency, Texture* outTexture)
{
	backend->CreateTexture(path, autoRelease, width, height, channelCount, pixels, hasTransparency, outTexture);
}

void RendererFrontend::DestroyTexture(Texture* texture)
{
	backend->DestroyTexture(texture);
}

bool RendererFrontend::DrawFrame(RenderPacket* packet)
{
	bool ret = true;

	// If the begin frame returned successfully, mid-frame operations may continue.
	if (BeginFrame(packet->deltaTime)) 
	{
		// Use Camera Attributes, passed along with renderpacket.
		UpdateGlobalState(packet->camera.GetProjectionMatrix(), packet->camera.GetViewMatrix(), packet->camera.GetPos(), float4::one, 0);

		// Angular velocity in radians per second.
		static constexpr float angularVelocity = 1.0f; // Adjust for desired speed

		// Accumulate the angle based on elapsed time (deltaTime).
		static float angle = 0.0f;
		angle += angularVelocity * packet->deltaTime;

		// Create the rotation matrix using the accumulated angle.
		float4x4 model = Quat(float3::unitZ, angle).ToFloat4x4();

		GeometryRenderData renderData{};
		renderData.objectID = 0;
		renderData.model = model;
		renderData.textures[0] = &testDiffuse;

		// Update the object's transform with the new model matrix.
		UpdateObject(renderData);

		// End of the frame. If this fails, it is likely unrecoverable.
		bool result = EndFrame(packet->deltaTime);

		if (!result) 
		{
			NOUS_ERROR("RendererFrontend::EndFrame() failed. Application shutting down...");
			ret = false;
		}
	}

	return ret;
}

void RendererFrontend::CreateDefaultTexture()
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
				pixels[indexBpp + 2] = 0; // Blue
				pixels[indexBpp + 3] = 255; // Alpha
			}
		}
	}

	CreateTexture("DefaultTexture", false, texDimension, texDimension, 4, pixels.data(), false, &defaultTexture);

	// Manually set the texture generation to invalid since this is a default texture.
	defaultTexture.generation = INVALID_ID;
}