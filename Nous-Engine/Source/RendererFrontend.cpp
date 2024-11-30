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

	// TODO: Make this configurable
	backend->Create(RendererBackendType::VULKAN);
	backend->frameNumber = 0;

	if (!backend->Initalize()) 
	{
		NOUS_FATAL("Renderer backend failed to initialize. Shutting down.");
		ret = false;
	}

	return ret;
}

void RendererFrontend::Shutdown()
{
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

void RendererFrontend::UpdateObject(float4x4 model)
{
	backend->UpdateObject(model);
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

		// Update the object's transform with the new model matrix.
		UpdateObject(model);

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
