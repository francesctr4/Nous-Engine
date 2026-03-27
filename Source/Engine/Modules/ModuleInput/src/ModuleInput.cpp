#include "Engine/Modules/ModuleInput/include/ModuleInput.h"
#include "Engine/Core/Application.h"
#include "Engine/Core/EventSystem/EventSystem.h"
#include "Engine/Core/Logger/Logger.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/Modules/ModuleWindow/include/ModuleWindow.h"

#include "SDL3/SDL.h"
#include "SDL3/SDL_vulkan.h"

static KeyState AdvanceKeyState(KeyState current, bool pressed)
{
	if (pressed)
		return (current == KeyState::IDLE) ? KeyState::DOWN : KeyState::REPEAT;
	return (current == KeyState::REPEAT || current == KeyState::DOWN) ? KeyState::UP : KeyState::IDLE;
}

ModuleInput::ModuleInput(Application* app, ModuleWindow* moduleWindow) : Module(app), mModuleWindow(moduleWindow)
{
	keyboard = NOUS_NEW_ARRAY<KeyState>(MAX_KEYBOARD_KEYS, MemoryTag::INPUT);

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
	NOUS_DELETE_ARRAY(keyboard, MAX_KEYBOARD_KEYS, MemoryTag::INPUT);
}

bool ModuleInput::Awake()
{
	bool ret = true;

	if (!SDL_InitSubSystem(SDL_INIT_EVENTS))
	{
		NOUS_ERROR("SDL_EVENTS could not initialize! SDL_Error: %s\n", SDL_GetError());
		ret = false;
	}

	return ret;
}

bool ModuleInput::Start()
{
	return true;
}

UpdateStatus ModuleInput::PreUpdate(float dt)
{
	UpdateStatus ret = UpdateStatus::CONTINUE;

	SDL_PumpEvents();

	// --------------- Handle Keyboard State --------------- \\
	
    const uint8* keys = reinterpret_cast<const uint8*>(SDL_GetKeyboardState(NULL));

	for (int i = 0; i < MAX_KEYBOARD_KEYS; ++i)
		keyboard[i] = AdvanceKeyState(keyboard[i], keys[i] == 1);

	// --------------- Handle Mouse State --------------- \\

    int32 buttons = SDL_GetMouseState(&mouseX, &mouseY);

	mouseZ = 0;

	for (int i = 0; i < MAX_MOUSE_BUTTONS; ++i)
		mouseButtons[i] = AdvanceKeyState(mouseButtons[i], (buttons & SDL_BUTTON_MASK(i)) != 0);

	mouseXMotion = 0;
	mouseYMotion = 0;

	// Handle SDL Input Events

	SDL_Event e;
	while (SDL_PollEvent(&e))
	{
        EventSystem->Broadcast(Event(EventType::INPUT_EVENT, SendContext(&e)));

		switch (e.type)
		{
			case SDL_EVENT_KEY_DOWN:
			{
				if (GetKey(SDL_SCANCODE_ESCAPE) == KeyState::DOWN)
				{
					ret = UpdateStatus::STOP;
				}

				break;
			}
			case SDL_EVENT_MOUSE_WHEEL:
			{
				mouseZ = e.wheel.y;
				break;
			}
			case SDL_EVENT_MOUSE_MOTION:
			{
				mouseX = e.motion.x;
				mouseY = e.motion.y;

				mouseXMotion = e.motion.xrel;
				mouseYMotion = e.motion.yrel;
				break;
			}
            case SDL_EVENT_WINDOW_RESIZED:
            {
                int width = e.window.data1;
                int height = e.window.data2;

                if (width != cachedFramebufferWidth || height != cachedFramebufferHeight)
                {
					cachedFramebufferWidth = width;
					cachedFramebufferHeight = height;

					EventSystem->Broadcast(Event(EventType::WINDOW_RESIZED, SendContext(cachedFramebufferWidth, cachedFramebufferHeight)));
                }

                break;
            }
            case SDL_EVENT_WINDOW_MINIMIZED:
			{
				mModuleWindow->SetMinimized(true);
                break;
            }
            case SDL_EVENT_WINDOW_RESTORED:
            {
				mModuleWindow->SetMinimized(false);
                break;
            }
			case SDL_EVENT_DROP_FILE:
			{ 
				const char* droppedFileDirectory = e.drop.data;
				EventSystem->Broadcast(Event(EventType::DROP_FILE, SendContext(e.drop.data)));
				SDL_free(&droppedFileDirectory);

				break;
			}
			case SDL_EVENT_QUIT:
			{
				ret = UpdateStatus::STOP;
				break;
			}
		}
	}

	return ret;
}

bool ModuleInput::CleanUp()
{
	SDL_QuitSubSystem(SDL_INIT_EVENTS);
	SDL_Quit();

	return true;
}

void ModuleInput::OnEvent(const Event& event)
{
	switch (event.type)
	{
		default: break;
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
