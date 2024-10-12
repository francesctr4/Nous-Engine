#include "ModuleInput.h"
#include "Logger.h"
#include "MemoryManager.h"

ModuleInput::ModuleInput(Application* app, std::string name, bool start_enabled) : Module(app, name, start_enabled)
{
	NOUS_TRACE("%s()", __FUNCTION__);

	keyboard = NOUS_NEW_ARRAY<KeyState>(MAX_KEYBOARD_KEYS, MemoryManager::MemoryTag::INPUT);

	MemoryManager::SetMemory(keyboard, static_cast<int32>(KeyState::IDLE), sizeof(KeyState) * MAX_KEYBOARD_KEYS);
	MemoryManager::SetMemory(mouseButtons, static_cast<int32>(KeyState::IDLE), sizeof(KeyState) * MAX_MOUSE_BUTTONS);

	mouseX = 0;
	mouseY = 0;
	mouseZ = 0;

	mouseXMotion = 0;
	mouseYMotion = 0;
}

ModuleInput::~ModuleInput()
{
	NOUS_TRACE("%s()", __FUNCTION__);

	NOUS_DELETE_ARRAY(keyboard, MAX_KEYBOARD_KEYS, MemoryManager::MemoryTag::INPUT);
}

bool ModuleInput::Awake()
{
	NOUS_TRACE("%s()", __FUNCTION__);

	bool ret = true;

	if (SDL_InitSubSystem(SDL_INIT_EVENTS) < 0)
	{
		NOUS_ERROR("SDL_EVENTS could not initialize! SDL_Error: %s\n", SDL_GetError());
		ret = false;
	}

	return ret;
}

bool ModuleInput::Start()
{
	NOUS_TRACE("%s()", __FUNCTION__);
	return true;
}

UpdateStatus ModuleInput::PreUpdate(float dt)
{
	NOUS_TRACE("%s()", __FUNCTION__);

	// Handle Keyboard State

	const uint8* keys = SDL_GetKeyboardState(NULL);

	for (int i = 0; i < MAX_KEYBOARD_KEYS; ++i)
	{
		if (keys[i] == 1)
		{
			if (keyboard[i] == KeyState::IDLE) 
			{
				keyboard[i] = KeyState::DOWN;
			}
			else 
			{
				keyboard[i] = KeyState::REPEAT;
			}
		}
		else
		{
			if (keyboard[i] == KeyState::REPEAT || keyboard[i] == KeyState::DOWN) 
			{
				keyboard[i] = KeyState::UP;
			}
			else 
			{
				keyboard[i] = KeyState::IDLE;
			}
		}
	}

	// Handle Mouse State

	/*uint32 buttons = SDL_GetMouseState(&mouseX, &mouseY);

	mouseZ = 0;

	for (int i = 0; i < 5; ++i)
	{
		if (buttons & SDL_BUTTON(i))
		{
			if (mouse_buttons[i] == KEY_IDLE)
				mouse_buttons[i] = KEY_DOWN;
			else
				mouse_buttons[i] = KEY_REPEAT;
		}
		else
		{
			if (mouse_buttons[i] == KEY_REPEAT || mouse_buttons[i] == KEY_DOWN)
				mouse_buttons[i] = KEY_UP;
			else
				mouse_buttons[i] = KEY_IDLE;
		}
	}*/

	mouseXMotion = 0;
	mouseYMotion = 0;

	// Handle SDL Input Events

	SDL_Event e;
	while (SDL_PollEvent(&e))
	{
		switch (e.type)
		{
			case SDL_KEYDOWN:
			{
				// Handle key press event
				/*KEY_STATE state = GetKey(e.key.keysym.scancode);
				App->BroadcastEvent(Event(EventType::KEY_PRESSED, { .int64 = {e.key.keysym.scancode, state }));*/

				break;
			}
			case SDL_KEYUP:
			{
				// Handle key release event
				/*KEY_STATE state = GetKey(e.key.keysym.scancode);
				App->BroadcastEvent(Event(EventType::KEY_RELEASED, { .int64 = {e.key.keysym.scancode, state }));*/
				
				break;
			}
			case SDL_MOUSEBUTTONDOWN:
			{
				// Handle mouse button press event
				/*KEY_STATE state = GetMouseButton(e.button.button);
				App->BroadcastEvent(Event(EventType::MOUSE_BUTTON_PRESSED, { .int32 = {e.button.button, state, e.button.x, e.button.y }));*/
				break;
			}
			case SDL_MOUSEBUTTONUP:
			{
				// Handle mouse button release event
				/*KEY_STATE state = GetMouseButton(e.button.button);
				App->BroadcastEvent(Event(EventType::MOUSE_BUTTON_RELEASED, { .int32 = {e.button.button, state, e.button.x, e.button.y }));*/
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

	SDL_QuitSubSystem(SDL_INIT_EVENTS);
	SDL_Quit();

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

KeyState ModuleInput::GetKey(int id) const
{
	return keyboard[id];
}

KeyState ModuleInput::GetMouseButton(int id) const
{
	return mouseButtons[id];
}

uint32 ModuleInput::GetMouseX() const
{
	return mouseX;
}

uint32 ModuleInput::GetMouseY() const
{
	return mouseY;
}

uint32 ModuleInput::GetMouseZ() const
{
	return mouseZ;
}

uint32 ModuleInput::GetMouseXMotion() const
{
	return mouseXMotion;
}

uint32 ModuleInput::GetMouseYMotion() const
{
	return mouseYMotion;
}
