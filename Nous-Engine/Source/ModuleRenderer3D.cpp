#include "ModuleRenderer3D.h"
#include "Logger.h"

#include "Tracy.h"

#include "RendererFrontend.h"

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

	rendererFrontend->DrawFrame(&packet);

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
	case EventType::TEST:
		/*NOUS_ERROR("ModuleRenderer3D LISTENED TEST");
		NOUS_ERROR("Received context: %d, %d", event.context.int64[0], event.context.int64[1]);*/
		break;

	case EventType::KEY_PRESSED:
	{
		if (event.context.int64[0] == 11 && event.context.int64[1] == 1)
		{
			NOUS_ERROR("ModuleInput Listened");
			NOUS_ERROR("Received context: %d, %d", event.context.int64[0], event.context.int64[1]);
		}

		break;
	}

	default:
		break;
	}
}
