#include "RendererFrontend.h"
#include "RendererBackend.h"

#include "MemoryManager.h"
#include "Logger.h"

#include "TextureSystem.h"
#include "MaterialSystem.h"
#include "GeometrySystem.h"

#include "ModuleEditor.h"

RendererFrontend::RendererFrontend()
{
	backendType = RendererBackendType::UNKNOWN;
	backend = NOUS_NEW<RendererBackend>(MemoryManager::MemoryTag::RENDERER);
}

RendererFrontend::~RendererFrontend()
{
	NOUS_DELETE(backend, MemoryManager::MemoryTag::RENDERER);
}

bool RendererFrontend::Initialize(RendererBackendType backendType)
{
	bool ret = true;

	this->backendType = backendType;

	// TODO: Make this configurable
	backend->Create(backendType);
	backend->frameNumber = 0;

	if (!backend->Initalize()) 
	{
		NOUS_FATAL("Renderer backend failed to initialize. Shutting down.");
		ret = false;
	}

	NOUS_TextureSystem::Initialize();
	NOUS_MaterialSystem::Initialize();
	NOUS_GeometrySystem::Initialize();

	return ret;
}

void RendererFrontend::Shutdown()
{
	NOUS_GeometrySystem::Shutdown();
	NOUS_MaterialSystem::Shutdown();
	NOUS_TextureSystem::Shutdown();

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

bool RendererFrontend::BeginRenderpass(BuiltInRenderpass renderpassID)
{
	return backend->BeginRenderpass(renderpassID);
}

bool RendererFrontend::EndRenderpass(BuiltInRenderpass renderpassID)
{
	return backend->EndRenderpass(renderpassID);
}

void RendererFrontend::UpdateGlobalWorldState(float4x4 projection, float4x4 view, float3 viewPosition, float4 ambientColor, int32 mode)
{
	backend->UpdateGlobalWorldState(projection, view, viewPosition, ambientColor, mode);
}

void RendererFrontend::UpdateGlobalUIState(float4x4 projection, float4x4 view, int32 mode)
{
	backend->UpdateGlobalUIState(projection, view, mode);
}

void RendererFrontend::DrawGeometry(GeometryRenderData renderData)
{
	backend->DrawGeometry(renderData);
}

void RendererFrontend::DrawEditor()
{
	External->editor->DrawEditor();
}

void RendererFrontend::CreateTexture(const uint8* pixels, ResourceTexture* outTexture)
{
	backend->CreateTexture(pixels, outTexture);
}

void RendererFrontend::DestroyTexture(ResourceTexture* texture)
{
	backend->DestroyTexture(texture);
}

bool RendererFrontend::CreateMaterial(ResourceMaterial* material)
{
	return backend->CreateMaterial(material);
}

void RendererFrontend::DestroyMaterial(ResourceMaterial* material)
{
	backend->DestroyMaterial(material);
}

bool RendererFrontend::CreateGeometry(uint32 vertexCount, const Vertex3D* vertices, uint32 indexCount, const uint32* indices, ResourceMesh* outGeometry)
{
	return backend->CreateGeometry(vertexCount, vertices, indexCount, indices, outGeometry);
}

void RendererFrontend::DestroyGeometry(ResourceMesh* geometry)
{
	backend->DestroyGeometry(geometry);
}

bool RendererFrontend::DrawFrame(RenderPacket* packet)
{
	bool ret = true;

	// If the begin frame returned successfully, mid-frame operations may continue.
	if (BeginFrame(packet->deltaTime)) 
	{
		// ----------------------------------------------------------------------------------------------------- //

		if (!BeginRenderpass(BuiltInRenderpass::WORLD))
		{
			NOUS_ERROR("BeginRenderpass WORLD failed! Application shutting down...");
			ret = false;
		}

		// Use Camera Attributes, passed along with renderpacket.
		UpdateGlobalWorldState(packet->camera.GetProjectionMatrix(), packet->camera.GetViewMatrix(), packet->camera.GetPos(), float4::one, 0);

		for (auto& geometry : packet->geometries)
		{
			DrawGeometry(geometry);
		}

		if (!EndRenderpass(BuiltInRenderpass::WORLD))
		{
			NOUS_ERROR("EndRenderpass WORLD failed! Application shutting down...");
			ret = false;
		}

		// ----------------------------------------------------------------------------------------------------- //

		if (!BeginRenderpass(BuiltInRenderpass::UI))
		{
			NOUS_ERROR("BeginRenderpass UI failed! Application shutting down...");
			ret = false;
		}

		UpdateGlobalUIState(packet->camera.GetProjectionMatrix(), packet->camera.GetViewMatrix(), 0);
		
		DrawEditor();

		if (!EndRenderpass(BuiltInRenderpass::UI))
		{
			NOUS_ERROR("EndRenderpass UI failed! Application shutting down...");
			ret = false;
		}

		// ----------------------------------------------------------------------------------------------------- //

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