#include "ModuleRenderer3D.h"
#include "ModuleCamera3D.h"

#include "Logger.h"
#include "MemoryManager.h";

#include "Tracy.h"

#include "RendererFrontend.h"
#include "ImporterTexture.h"

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

	if (!rendererFrontend->Initialize()) 
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

	// TODO: Refactor packet creation
	RenderPacket packet;

	packet.deltaTime = dt;
	packet.camera = App->camera->GetCamera();

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
		case EventType::SWAP_TEXTURE: 
		{
			const char* names[3] = { "cobblestone", "paving", "paving2" };
			static int8 choice = 2;
			choice++;
			choice %= 3;

			std::string path = std::format("Assets/Textures/{}.{}", names[choice], "png");

			// Load up the new texture.
			ImporterTexture::Import(path.c_str(), &testDiffuse);

			break;
		}
		case EventType::KEY_PRESSED:
		{
			/*if (event.context.int64[0] == 11 && event.context.int64[1] == 1)
			{
				NOUS_ERROR("ModuleInput Listened");
				NOUS_ERROR("Received context: %d, %d", event.context.int64[0], event.context.int64[1]);
			}*/

			break;
		}
		default: 
		{
			break;
		}
	}
}
