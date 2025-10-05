#include "Modules/ModuleRenderer3D.h"
#include "Modules/ModuleCamera3D.h"
#include "Modules/ModuleScene.h"

#include "Utils/Logger.h"
#include "Systems/Memory Manager/MemoryManager.h"

#include "Includes/Tracy.h"

#include "Renderer/RendererFrontend.h"
#include "Systems/Resource Manager/Importers/ImporterMesh.h"

#include "Modules/ModuleResourceManager.h"
#include "Systems/Resource Manager/Resource Types/ResourceMaterial.h"
#include "Systems/Resource Manager/Resource Types/ResourceTexture.h"
#include "Systems/Resource Manager/Resource Types/ResourceMesh.h"

#include "Includes/glmath.h"

#include "ECS/Scene.h"
#include "ECS/Components/ComponentMesh.h"
#include "ECS/Components/ComponentTransform.h"
#include "ECS/Components/ComponentMaterial.h"

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

    glm::quat rotation = glm::angleAxis(angle, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 model = glm::toMat4(rotation);

	/*
	TODO: Refactor a, por cada gameobject con mesh component con contenido válido, testrendergeometry es la cmesh
	y para el model le pasamos el ctransform del gameobject. de esta manera desde elinspector podemos cambiar
	el model de cada mesh. de forma independiente. Deberiamos separar el material de la mesh en si, para que se
	aplique luego despues en el vulkan y no aquí.
	*/

	for (const auto& goPtr : App->scene->activeScene->GetGameObjects())
	{
		GameObject* go = goPtr.get();

		GeometryRenderData data{};
		// Skip objects without the required components

		if (go->HasComponent<CTransform>())
		{
			auto& ctransform = go->GetComponent<CTransform>();
			data.model = ctransform.worldMatrix;   // or transform.GetMatrix()
		}

		if (go->HasComponent<CMesh>()){
			auto& cmesh= go->GetComponent<CMesh>();
			data.geometry = cmesh.mesh;
		}

		if (go->HasComponent<CMaterial>()) {
			auto &cmaterial = go->GetComponent<CMaterial>();
			data.material = cmaterial.material;
		}

		packet.geometries.push_back(data);
	}

//	for (const auto& [UID, Resource] : App->resourceManager->GetResourcesMap())
//	{
//		if (Resource->GetType() == ResourceType::MESH)
//		{
//			GeometryRenderData testRender{};
//			testRender.geometry = static_cast<ResourceMesh*>(Resource);
//			testRender.model = model;
//
//			packet.geometries.push_back(testRender);
//		}
//	}

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
			NOUS_DEBUG("Received context: %d, %d", event.context._i64[0], event.context._i64[1]);

			rendererFrontend->OnResized(event.context._i64[0], event.context._i64[1]);

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
