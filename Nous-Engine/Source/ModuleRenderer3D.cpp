#include "ModuleRenderer3D.h"
#include "ModuleCamera3D.h"
#include "ModuleScene.h"

#include "Logger.h"
#include "MemoryManager.h";

#include "Tracy.h"

#include "RendererFrontend.h"
#include "TextureSystem.h"
#include "GeometrySystem.h"
#include "ImporterMesh.h"

#include "ModuleResourceManager.h"
#include "ResourceMaterial.h"
#include "ResourceTexture.h"
#include "ResourceMesh.h"

RendererFrontend* ModuleRenderer3D::rendererFrontend = nullptr;

// Temp
ResourceMesh* testGeometry = nullptr;
// End Temp

ModuleRenderer3D::ModuleRenderer3D(Application* app) : Module(app)
{
	NOUS_TRACE("%s()", __FUNCTION__);

	rendererFrontend = NOUS_NEW<RendererFrontend>(MemoryManager::MemoryTag::RENDERER);
}

ModuleRenderer3D::~ModuleRenderer3D()
{
	NOUS_TRACE("%s()", __FUNCTION__);

	rendererFrontend->Shutdown();

	NOUS_DELETE(rendererFrontend, MemoryManager::MemoryTag::RENDERER);
}

bool ModuleRenderer3D::Awake()
{
	NOUS_TRACE("%s()", __FUNCTION__);

	bool ret = true;

	rendererFrontend->backendType = RendererBackendType::VULKAN;

	if (!rendererFrontend->Initialize(rendererFrontend->backendType))
	{
		NOUS_FATAL("Failed to initialize renderer. Aborting application.");
		ret = false;
	}

	return ret;
}

bool ModuleRenderer3D::Start()
{
	NOUS_TRACE("%s()", __FUNCTION__);
	
	return true;
}

UpdateStatus ModuleRenderer3D::PreUpdate(float dt)
{
	NOUS_TRACE("%s()", __FUNCTION__);

	return UPDATE_CONTINUE;
}

UpdateStatus ModuleRenderer3D::Update(float dt)
{
	NOUS_TRACE("%s()", __FUNCTION__);

	return UPDATE_CONTINUE;
}

UpdateStatus ModuleRenderer3D::PostUpdate(float dt)
{
	NOUS_TRACE("%s()", __FUNCTION__);

	RenderPacket packet;

	// TODO: Refactor packet creation
	packet.deltaTime = dt;
	packet.editorCamera = *App->camera->GetCamera();
	packet.gameCamera = *App->scene->gameCamera;

	// Angular velocity in radians per second.
	static constexpr float angularVelocity = 1.0f; // Adjust for desired speed

	// Accumulate the angle based on elapsed time (deltaTime).
	static float angle = 0.0f;
	angle += angularVelocity * packet.deltaTime;

	// Create the rotation matrix using the accumulated angle.
	float4x4 model = Quat(float3::unitY, angle).ToFloat4x4();

	for (const auto& [UID, Resource] : App->resourceManager->GetResourcesMap()) 
	{
		if (Resource->GetType() == ResourceType::MESH) 
		{
			GeometryRenderData testRender;
			testRender.geometry = static_cast<ResourceMesh*>(Resource);
			testRender.model = model;

			packet.geometries.push_back(testRender);
		}
	}

	// TODO: end temp

	if (!App->isMinimized)
	{
		rendererFrontend->DrawFrame(&packet);
	}

	return UPDATE_CONTINUE;
}

bool ModuleRenderer3D::CleanUp()
{
	NOUS_TRACE("%s()", __FUNCTION__);

	bool ret = true;

	//NOUS_GeometrySystem::ReleaseGeometry(testGeometry);

	return ret;
}

void ModuleRenderer3D::ReceiveEvent(const Event& event)
{
	switch (event.type)
	{
		case EventType::WINDOW_RESIZED:
		{
			NOUS_DEBUG("%s() --> WINDOW RESIZED EVENT", __FUNCTION__);
			NOUS_DEBUG("Received context: %d, %d", event.context.int64[0], event.context.int64[1]);

			rendererFrontend->OnResized(event.context.int64[0], event.context.int64[1]);

			break;
		}
		case EventType::DROP_FILE:
		{
			// Load up the new texture.
			//ImporterTexture::Import(event.context.c, rendererFrontend->testDiffuse);

			//rendererFrontend->testMaterial->diffuseMap.texture = NOUS_TextureSystem::AcquireTexture(event.context.c, true);

			//if (!rendererFrontend->testMaterial->diffuseMap.texture)
			//{
			//	NOUS_WARN("event_on_debug_event no texture! using default");
			//	rendererFrontend->testMaterial->diffuseMap.texture = NOUS_TextureSystem::GetDefaultTexture();
			//}

			// Acquire the new texture.
			//if (testGeometry) 
			//{
			//	testGeometry->material->diffuseMap.texture = NOUS_TextureSystem::AcquireTexture(event.context.c, true);
			//	//ImporterTexture::Import(event.context.c, testGeometry->material->diffuseMap.texture);

			//	if (!testGeometry->material->diffuseMap.texture) 
			//	{
			//		NOUS_WARN("event_on_debug_event no texture! using default");
			//		testGeometry->material->diffuseMap.texture = NOUS_TextureSystem::GetDefaultTexture();
			//	}
			//}

			break;
		}
		default: 
		{
			break;
		}
	}
}
