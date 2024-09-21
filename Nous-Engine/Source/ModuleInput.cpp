#include "ModuleInput.h"
#include "Logger.h"

ModuleInput::ModuleInput(Application* app, std::string name, bool start_enabled) : Module(app, name, start_enabled)
{
	NOUS_TRACE("%s()", __FUNCTION__);
}

ModuleInput::~ModuleInput()
{
	NOUS_TRACE("%s()", __FUNCTION__);
}

bool ModuleInput::Awake()
{
	NOUS_TRACE("%s()", __FUNCTION__);
	return true;
}

bool ModuleInput::Start()
{
	NOUS_TRACE("%s()", __FUNCTION__);
	return true;
}

UpdateStatus ModuleInput::PreUpdate(float dt)
{
	NOUS_TRACE("%s()", __FUNCTION__);

	// Make the system sleep for 2 seconds
	std::this_thread::sleep_for(std::chrono::seconds(2));

	SDL_Event e;
	while (SDL_PollEvent(&e))
	{
		switch (e.type)
		{
			case SDL_QUIT:
			{
				return UPDATE_STOP;
				break;
			}
		}
	}

	return UPDATE_CONTINUE;
}

bool ModuleInput::CleanUp()
{
	NOUS_TRACE("%s()", __FUNCTION__);

	return true;
}
