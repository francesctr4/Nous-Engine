#include "RendererFrontend.h"
#include "Renderer/Backend/RendererBackend.h"
#include "Core/Application.h"
#include "Systems/Memory Manager/MemoryManager.h"
#include "Utils/Logger.h"

#include "Systems/Texture System/TextureSystem.h"
#include "Systems/Material System/MaterialSystem.h"
#include "Systems/Geometry System/GeometrySystem.h"
#include "Systems/Camera System/Camera.h"

#include "Core/Modules/ModuleEditor.h"

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
		NOUS_FATAL("[%s] Renderer backend failed to initialize. Shutting down.", __FUNCTION__);
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

bool RendererFrontend::BeginRenderpass(RenderpassType renderpassID)
{
	return backend->BeginRenderpass(renderpassID);
}

bool RendererFrontend::EndRenderpass(RenderpassType renderpassID)
{
	return backend->EndRenderpass(renderpassID);
}

void RendererFrontend::UpdateGlobalWorldState(RenderpassType renderpassID, glm::mat4x4 projection, glm::mat4x4 view, glm::vec3 viewPosition, glm::vec4 ambientColor, int32 mode)
{
	backend->UpdateGlobalWorldState(renderpassID, projection, view, viewPosition, ambientColor, mode);
}

void RendererFrontend::UpdateGlobalUIState(RenderpassType renderpassID, glm::mat4x4 projection, glm::mat4x4 view, int32 mode)
{
	backend->UpdateGlobalUIState(renderpassID, projection, view, mode);
}

void RendererFrontend::DrawGeometry(RenderpassType renderpassID, GeometryRenderData renderData)
{
	backend->DrawGeometry(renderpassID, renderData);
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

		if (!BeginRenderpass(RenderpassType::SCENE))
		{
			NOUS_ERROR("BeginRenderpass SCENE failed! Application shutting down...");
			ret = false;
		}

		// Use Camera Attributes, passed along with renderpacket.
		UpdateGlobalWorldState(RenderpassType::SCENE, packet->editorCamera->GetProjectionMatrix(), packet->editorCamera->GetViewMatrix(), packet->editorCamera->GetPos(), glm::vec4(1.0f), 0);

		for (auto& geometry : packet->geometries)
		{
			DrawGeometry(RenderpassType::SCENE, geometry);
		}

		// DrawGameCamera();
		// DrawGrid();

		if (!EndRenderpass(RenderpassType::SCENE))
		{
			NOUS_ERROR("EndRenderpass SCENE failed! Application shutting down...");
			ret = false;
		}

		// ----------------------------------------------------------------------------------------------------- //

		if (!BeginRenderpass(RenderpassType::GAME))
		{
			NOUS_ERROR("BeginRenderpass GAME failed! Application shutting down...");
			ret = false;
		}

		// Use Camera Attributes, passed along with renderpacket.
		UpdateGlobalWorldState(RenderpassType::GAME, packet->gameCamera->GetProjectionMatrix(), packet->gameCamera->GetViewMatrix(), packet->gameCamera->GetPos(), glm::vec4(1.0f), 0);

		for (auto& geometry : packet->geometries)
		{
			DrawGeometry(RenderpassType::GAME, geometry);
		}

		if (!EndRenderpass(RenderpassType::GAME))
		{
			NOUS_ERROR("EndRenderpass GAME failed! Application shutting down...");
			ret = false;
		}

		// ----------------------------------------------------------------------------------------------------- //

		if (!BeginRenderpass(RenderpassType::UI))
		{
			NOUS_ERROR("BeginRenderpass UI failed! Application shutting down...");
			ret = false;
		}

		UpdateGlobalUIState(RenderpassType::UI, packet->editorCamera->GetProjectionMatrix(), packet->editorCamera->GetViewMatrix(), 0);
		
		DrawEditor();

		if (!EndRenderpass(RenderpassType::UI))
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