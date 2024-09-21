#include "ModuleInput.h"

ModuleInput::ModuleInput(Application* app, std::string name, bool start_enabled) : Module(app, name, start_enabled)
{

}

ModuleInput::~ModuleInput()
{

}

bool ModuleInput::Awake()
{
	return true;
}

bool ModuleInput::Start()
{
	return true;
}

UpdateStatus ModuleInput::PreUpdate(float dt)
{
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
	return true;
}
