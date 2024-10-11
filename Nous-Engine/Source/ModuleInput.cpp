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

	SDL_Event e;
	while (SDL_PollEvent(&e))
	{
		switch (e.type)
		{
			case SDL_KEYDOWN:
			{
				// Handle key press event
				KEY_STATE state = GetKey(e.key.keysym.scancode);
				App->BroadcastEvent(Event(EventType::KEY_PRESSED, { .int64 = {e.key.keysym.scancode, state }));

				break;
			}
			case SDL_KEYUP:
			{
				// Handle key release event
				KEY_STATE state = GetKey(e.key.keysym.scancode);
				App->BroadcastEvent(Event(EventType::KEY_RELEASED, { .int64 = {e.key.keysym.scancode, state }));
				
				break;
			}
			case SDL_MOUSEBUTTONDOWN:
			{
				// Handle mouse button press event
				KEY_STATE state = GetMouseButton(e.button.button);
				App->BroadcastEvent(Event(EventType::MOUSE_BUTTON_PRESSED, { .int32 = {e.button.button, state, e.button.x, e.button.y }));
				break;
			}
			case SDL_MOUSEBUTTONUP:
			{
				// Handle mouse button release event
				KEY_STATE state = GetMouseButton(e.button.button);
				App->BroadcastEvent(Event(EventType::MOUSE_BUTTON_RELEASED, { .int32 = {e.button.button, state, e.button.x, e.button.y }));
				break;
			}
			case SDL_MOUSEWHEEL:
			{
				//int32 mouse_z = e.wheel.y;
				break;
			}
			case SDL_MOUSEMOTION:
			{
				//int mouse_x = e.motion.x;
				//int mouse_y = e.motion.y;

				//int mouse_x_motion = e.motion.xrel;
				//int mouse_y_motion = e.motion.yrel;
				break;
			}
			case SDL_WINDOWEVENT:
			{
				switch (e.window.event) 
				{
					case SDL_WINDOWEVENT_RESIZED:
					{
						//int width = e.window.data1;
						//int height = e.window.data2;

						break;
					}
					case SDL_WINDOWEVENT_MAXIMIZED:
					{
						//int width = e.window.data1;
						//int height = e.window.data2;

						break;
					}
				}
				break;
			}
			case SDL_DROPFILE:
			{ 
				// Broadcast Event (example)
				
				//const char* droppedFileDirectory = e.drop.file;
				//App->BroadcastEvent(Event(EventType::DROPFILE, { .c = *e.drop.file }));
				//SDL_free(&droppedFileDirectory);

				break;
			}
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

void ModuleInput::ReceiveEvent(const Event& event)
{
	switch (event.type)
	{
	default:
		break;
	}
}
