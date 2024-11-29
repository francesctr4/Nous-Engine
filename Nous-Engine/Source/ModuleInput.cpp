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

	UpdateStatus ret = UPDATE_CONTINUE;

	SDL_PumpEvents();

	// --------------- Handle Keyboard State --------------- \\
	
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

	// --------------- Handle Mouse State --------------- \\

	int32 buttons = SDL_GetMouseState(&mouseX, &mouseY);

	mouseZ = 0;

	for (int i = 0; i < 5; ++i)
	{
		if (buttons & SDL_BUTTON(i))
		{
			if (mouseButtons[i] == KeyState::IDLE) 
			{
				mouseButtons[i] = KeyState::DOWN;
			}
			else 
			{
				mouseButtons[i] = KeyState::REPEAT;
			}	
		}
		else
		{
			if (mouseButtons[i] == KeyState::REPEAT || mouseButtons[i] == KeyState::DOWN)
			{
				mouseButtons[i] = KeyState::UP;
			}
			else 
			{
				mouseButtons[i] = KeyState::IDLE;
			}
		}
	}

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
				/*KeyState state = GetKey(e.key.keysym.scancode);
				App->BroadcastEvent(Event(EventType::KEY_PRESSED, { .int64 = {e.key.keysym.scancode, static_cast<int64>(state) }}));*/

				if (GetKey(SDL_SCANCODE_ESCAPE) == KeyState::DOWN)
				{
					ret = UPDATE_STOP;
				}

				break;
			}
			case SDL_KEYUP:
			{
				// Handle key release event
				/*KEY_STATE state = GetKey(e.key.keysym.scancode);
				App->BroadcastEvent(Event(EventType::KEY_RELEASED, { .int64 = {e.key.keysym.scancode, state }));*/
				//NOUS_ERROR(" ");
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
				mouseZ = e.wheel.y;
				break;
			}
			case SDL_MOUSEMOTION:
			{
				mouseX = e.motion.x;
				mouseX = e.motion.y;

				mouseXMotion = e.motion.xrel;
				mouseYMotion = e.motion.yrel;
				break;
			}
			case SDL_WINDOWEVENT:
			{
				switch (e.window.event) 
				{
					case SDL_WINDOWEVENT_RESIZED:
					{
						int width = e.window.data1;
						int height = e.window.data2;

						if (width != cachedFramebufferWidth || height != cachedFramebufferHeight)
						{
							cachedFramebufferWidth = width;
							cachedFramebufferHeight = height;

							App->BroadcastEvent(Event(EventType::WINDOW_RESIZED, { .int64 = { cachedFramebufferWidth, cachedFramebufferHeight } }));
						}
						
						break;
					}
					case SDL_WINDOWEVENT_MINIMIZED:
					{
						App->isMinimized = true;
						break;
					}
					case SDL_WINDOWEVENT_RESTORED:
					{
						App->isMinimized = false;
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
				ret = UPDATE_STOP;
				break;
			}
		}
	}

	return ret;
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

int32 ModuleInput::GetMouseX() const
{
	return mouseX;
}

int32 ModuleInput::GetMouseY() const
{
	return mouseY;
}

int32 ModuleInput::GetMouseZ() const
{
	return mouseZ;
}

int32 ModuleInput::GetMouseXMotion() const
{
	return mouseXMotion;
}

int32 ModuleInput::GetMouseYMotion() const
{
	return mouseYMotion;
}
