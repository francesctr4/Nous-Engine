#include "ModuleRenderer3D.h"
#include "ModuleCamera3D.h"

#include "Logger.h"
#include "MemoryManager.h";

#include "Tracy.h"

#include "RendererFrontend.h"
#include "TextureSystem.h"
#include "GeometrySystem.h"
#include "ImporterMesh.h"

RendererFrontend* ModuleRenderer3D::rendererFrontend = nullptr;

// Temp
Geometry* testGeometry;
// End Temp

ModuleRenderer3D::ModuleRenderer3D(Application* app, std::string name, bool start_enabled) : Module(app, name, start_enabled)
{
	NOUS_TRACE("%s()", __FUNCTION__);

	rendererFrontend = NOUS_NEW<RendererFrontend>(MemoryManager::MemoryTag::RENDERER);
}

ModuleRenderer3D::~ModuleRenderer3D()
{
	NOUS_TRACE("%s()", __FUNCTION__);

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

	Mesh* myMesh = NOUS_NEW<Mesh>(MemoryManager::MemoryTag::GAME);

	ImporterMesh::Import("Assets/Meshes/Cypher_S0_Skelmesh.fbx", myMesh);
	ImporterMesh::Save("Library/Meshes/Cypher_S0_Skelmesh.nmesh", *myMesh);

	NOUS_DELETE(myMesh, MemoryManager::MemoryTag::GAME);
	myMesh = NOUS_NEW<Mesh>(MemoryManager::MemoryTag::GAME);

	ImporterMesh::Load("Library/Meshes/Cypher_S0_Skelmesh.nmesh", myMesh);

	/*GeometryConfig gConfig = NOUS_GeometrySystem::GeneratePlaneConfig(10.0f, 5.0f, 5, 5, 5.0f, 2.0f, "test_geometry", "test_material");*/
	GeometryConfig gConfig;
	gConfig.name = "Cypher";
	gConfig.materialPath = "DefaultMaterial";
	gConfig.vertices = myMesh->vertices;
	gConfig.indices = myMesh->indices;

	testGeometry = NOUS_GeometrySystem::AcquireFromConfig(gConfig, true);

	NOUS_DELETE(myMesh, MemoryManager::MemoryTag::GAME);

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

	// TODO: Refactor packet creation
	RenderPacket packet;

	packet.deltaTime = dt;
	packet.camera = *App->camera->GetCamera();

	// TODO: temp
	GeometryRenderData testRender;
	testRender.geometry = testGeometry;

	// Angular velocity in radians per second.
	static constexpr float angularVelocity = 1.0f; // Adjust for desired speed

	// Accumulate the angle based on elapsed time (deltaTime).
	static float angle = 0.0f;
	angle += angularVelocity * packet.deltaTime;

	// Create the rotation matrix using the accumulated angle.
	float4x4 model = Quat(float3::unitY, angle).ToFloat4x4();

	testRender.model = model;

	packet.geometries.push_back(testRender);
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

	NOUS_GeometrySystem::ReleaseGeometry(testGeometry);

	rendererFrontend->Shutdown();

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
			if (testGeometry) 
			{
				testGeometry->material->diffuseMap.texture = NOUS_TextureSystem::AcquireTexture(event.context.c, true);
				//ImporterTexture::Import(event.context.c, testGeometry->material->diffuseMap.texture);

				if (!testGeometry->material->diffuseMap.texture) 
				{
					NOUS_WARN("event_on_debug_event no texture! using default");
					testGeometry->material->diffuseMap.texture = NOUS_TextureSystem::GetDefaultTexture();
				}
			}

			break;
		}
		default: 
		{
			break;
		}
	}
}
