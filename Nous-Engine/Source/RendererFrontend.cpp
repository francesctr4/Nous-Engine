#include "RendererFrontend.h"
#include "RendererBackend.h"

#include "MemoryManager.h"
#include "Logger.h"

#include "TextureSystem.h"
#include "MaterialSystem.h"

#include "ModuleEditor.h"

RendererFrontend::RendererFrontend()
{
	backendType = RendererBackendType::UNKNOWN;
	backend = NOUS_NEW<RendererBackend>(MemoryManager::MemoryTag::RENDERER);

	testMaterial = nullptr;
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

	return ret;
}

void RendererFrontend::Shutdown()
{
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

		// Angular velocity in radians per second.
		static constexpr float angularVelocity = 1.0f; // Adjust for desired speed

		// Accumulate the angle based on elapsed time (deltaTime).
		static float angle = 0.0f;
		angle += angularVelocity * packet->deltaTime;

		// Create the rotation matrix using the accumulated angle.
		float4x4 model = Quat(float3::unitY, angle).ToFloat4x4();

		GeometryRenderData renderData{};
		renderData.model = model;

		if (!testMaterial) 
		{
			testMaterial = NOUS_MaterialSystem::AcquireMaterial("Assets/Materials/test_material.nmat");

			if (!testMaterial)
			{
				NOUS_WARN("Automatic material load failed, falling back to manual default material.");

				MaterialConfig config;

				config.name = "DefaultMaterial";
				config.autoRelease = false;
				config.diffuseColor = float4::one; // white
				config.diffuseMapName = NOUS_TextureSystem::state.config.DEFAULT_TEXTURE_NAME;

				testMaterial = NOUS_MaterialSystem::AcquireMaterialFromConfig(config);
				//testMaterial = NOUS_MaterialSystem::GetDefaultMaterial();
			}
		}

		renderData.material = testMaterial;

		// Update the object's transform with the new model matrix.
		DrawGeometry(renderData);

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