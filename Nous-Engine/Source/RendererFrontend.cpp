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

void RendererFrontend::UpdateGlobalState(float4x4 projection, float4x4 view, float3 viewPosition, float4 ambientColor, int32 mode)
{
	backend->UpdateGlobalState(projection, view, viewPosition, ambientColor, mode);
}

void RendererFrontend::DrawGeometry(GeometryRenderData renderData)
{
	backend->DrawGeometry(renderData);
}

void RendererFrontend::CreateTexture(const uint8* pixels, Texture* outTexture)
{
	backend->CreateTexture(pixels, outTexture);
}

void RendererFrontend::DestroyTexture(Texture* texture)
{
	backend->DestroyTexture(texture);
}

bool RendererFrontend::CreateMaterial(Material* material)
{
	return backend->CreateMaterial(material);
}

void RendererFrontend::DestroyMaterial(Material* material)
{
	backend->DestroyMaterial(material);
}

bool RendererFrontend::CreateGeometry(uint32 vertexCount, const Vertex* vertices, uint32 indexCount, const uint32* indices, Geometry* outGeometry)
{
	return backend->CreateGeometry(vertexCount, vertices, indexCount, indices, outGeometry);
}

void RendererFrontend::DestroyGeometry(Geometry* geometry)
{
	backend->DestroyGeometry(geometry);
}

bool RendererFrontend::DrawFrame(RenderPacket* packet)
{
	bool ret = true;

	// If the begin frame returned successfully, mid-frame operations may continue.
	if (BeginFrame(packet->deltaTime)) 
	{
		// Use Camera Attributes, passed along with renderpacket.
		UpdateGlobalState(packet->camera.GetProjectionMatrix(), packet->camera.GetViewMatrix(), packet->camera.GetPos(), float4::one, 0);

		for (auto& geometry : packet->geometries)
		{
			DrawGeometry(geometry);
		}

		// TODO: This shouldn't be here. The editor should have its own resources.
		//External->editor->DrawEditor();

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