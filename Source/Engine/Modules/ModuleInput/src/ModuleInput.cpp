#include <ModuleInput/ModuleInput.h>
#include <EventSystem/EventSystem.h>
#include <Logger/Logger.h>
#include <MemoryManager/MemoryManager.h>

#include "SDL3/SDL.h"
#include "SDL3/SDL_vulkan.h"

// Uncomment to log every key press to the console (useful for identifying scancodes)
#define NOUS_DEBUG_INPUT_LOG

#ifdef _PROFILING
#include <tracy/Tracy.hpp>
#endif

static KeyState AdvanceKeyState(KeyState current, bool pressed)
{
	if (pressed)
		return (current == KeyState::IDLE) ? KeyState::DOWN : KeyState::REPEAT;
	return (current == KeyState::REPEAT || current == KeyState::DOWN) ? KeyState::UP : KeyState::IDLE;
}

ModuleInput::ModuleInput(EventSystem* eventSystem, nous::engine::multithreading::NOUS_JobSystem* jobSystem)
    : Module(eventSystem, jobSystem)
{
	keyboard = NOUS_NEW_ARRAY<KeyState>(MAX_KEYBOARD_KEYS, MemoryTag::INPUT);

	nous::engine::memory::SetMemory(keyboard, static_cast<int32_t>(KeyState::IDLE), sizeof(KeyState) * MAX_KEYBOARD_KEYS);
	nous::engine::memory::SetMemory(mouseButtons, static_cast<int32_t>(KeyState::IDLE), sizeof(mouseButtons));

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
#ifdef _PROFILING
	ZoneScopedN("ModuleInput::PreUpdate");
#endif
	UpdateStatus ret = UpdateStatus::CONTINUE;

	SDL_PumpEvents();

	// --------------- Handle Keyboard State --------------- \\

    const uint8_t* keys = reinterpret_cast<const uint8_t*>(SDL_GetKeyboardState(NULL));

	for (int i = 0; i < MAX_KEYBOARD_KEYS; ++i)
		keyboard[i] = m_imguiCaptureKeyboard ? AdvanceKeyState(keyboard[i], false) : AdvanceKeyState(keyboard[i], keys[i] == 1);

	// --------------- Handle Mouse State --------------- \\

    int32_t buttons = SDL_GetMouseState(&mouseX, &mouseY);

	mouseZ = 0;

	// From 1, not 0: SDL button numbers are 1-based and SDL_BUTTON_MASK(X) is
	// (1u << ((X)-1)), so i == 0 shifted by -1 -- undefined behaviour. Debug got away
	// with it (x86 masks the count to 31, so the bit was simply never set), but at /Ob2
	// clang unrolls this loop, constant-folds the bad iteration to `unreachable`, and
	// emits an int3 that DISCARDS THE REST OF PreUpdate. That was an instant 0x80000003
	// on the first frame in Release only.
	for (int i = 1; i <= MAX_MOUSE_BUTTONS; ++i)
		mouseButtons[i] = AdvanceKeyState(mouseButtons[i], (buttons & SDL_BUTTON_MASK(i)) != 0);

	mouseXMotion = 0;
	mouseYMotion = 0;

	// Handle SDL Input Events

	SDL_Event e;
	while (SDL_PollEvent(&e))
	{
        eventSystem->Broadcast(Event(EventType::INPUT_EVENT, SendContext(&e)));

		switch (e.type)
		{
			case SDL_EVENT_KEY_DOWN:
			{
				// Escape quits the EDITOR only — it is a dev convenience, and the
				// editor has no script that could want the key.
				//
				// In a standalone game Escape belongs to the game: the shipped
				// camera script (and the template's own example) uses it to release
				// mouse capture, so stealing it here killed the app the first time
				// the player tried to get their cursor back. A game still quits
				// through SDL_EVENT_QUIT below (window close / Alt-F4); if a game
				// wants to quit from script, that needs an EngineAPI binding rather
				// than a hard-coded key.
				if (!m_gameMode && GetKey(SDL_SCANCODE_ESCAPE) == KeyState::DOWN)
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

                if (width != m_lastWindowWidth || height != m_lastWindowHeight)
                {
					m_lastWindowWidth  = width;
					m_lastWindowHeight = height;

					eventSystem->Broadcast(Event(EventType::WINDOW_RESIZED, SendContext(width, height)));
                }

                break;
            }
            case SDL_EVENT_WINDOW_MINIMIZED:
			{
				eventSystem->Broadcast(Event(EventType::WINDOW_MINIMIZED, SendContext(true)));
                break;
            }
            case SDL_EVENT_WINDOW_RESTORED:
            {
				eventSystem->Broadcast(Event(EventType::WINDOW_MINIMIZED, SendContext(false)));
                break;
            }
			case SDL_EVENT_DROP_FILE:
			{
				eventSystem->Broadcast(Event(EventType::DROP_FILE, SendContext(e.drop.data)));
				break;
			}
			case SDL_EVENT_QUIT:
			{
				ret = UpdateStatus::STOP;
				break;
			}
		}
	}


#ifdef NOUS_DEBUG_INPUT_LOG
	for (int i = 0; i < MAX_KEYBOARD_KEYS; ++i)
	{
		if (keyboard[i] == KeyState::DOWN)
			NOUS_TRACE("[ModuleInput] Key DOWN — scancode %d (%s)", i, SDL_GetScancodeName(static_cast<SDL_Scancode>(i)));
	}
	for (int i = 1; i <= MAX_MOUSE_BUTTONS; ++i)
	{
		if (mouseButtons[i] == KeyState::DOWN)
			NOUS_TRACE("[ModuleInput] Mouse button DOWN — button %d", i);
	}
#endif

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

// Both accessors are reached from SCRIPT with a raw int (InputBindings' GetKey /
// GetMouseButton forward whatever the script passes), so the index is untrusted and
// an out-of-range one is an out-of-bounds read of engine memory. Report IDLE instead
// -- the same answer a key nobody is pressing gives, so a script asking for a
// nonexistent button reads as "not pressed" rather than as garbage.

KeyState ModuleInput::GetKey(int id) const
{
	if (id < 0 || id >= MAX_KEYBOARD_KEYS)
		return KeyState::IDLE;

	return keyboard[id];
}

KeyState ModuleInput::GetMouseButton(int id) const
{
	// 1-based: see the mouseButtons declaration. Slot 0 exists only as padding.
	if (id < 1 || id > MAX_MOUSE_BUTTONS)
		return KeyState::IDLE;

	return mouseButtons[id];
}

void ModuleInput::SetImGuiCaptureKeyboard(bool captured)
{
	m_imguiCaptureKeyboard = captured;
}

void ModuleInput::SetMouseCaptured(bool captured)
{
	// In editor mode keep the OS cursor visible (ImGui panels need it) but still record the
	// logical capture state so script logic — IsMouseCaptured() checks, Escape toggles,
	// mouse-look-gated-on-capture branches — behaves identically to game mode.
	if (!m_gameMode)
	{
		m_mouseCaptured = captured;
		return;
	}

	// SDL3 made relative-mouse-mode per-window. We can't rely on SDL_GetMouseFocus()
	// because at the moment a script's Start() runs (e.g. right after launching a
	// standalone game), the cursor may not yet be over the new window — focus is
	// nullptr and capture would silently no-op. Targeting the first open window
	// via SDL_GetWindows() works regardless of focus state.
	int windowCount = 0;
	SDL_Window** windows = SDL_GetWindows(&windowCount);
	if (!windows || windowCount == 0)
	{
		NOUS_WARN("SetMouseCaptured: no SDL window open — ignoring");
		if (windows) SDL_free(windows);
		return;
	}

	const bool ok = SDL_SetWindowRelativeMouseMode(windows[0], captured);
	SDL_free(windows);

	if (!ok)
	{
		NOUS_ERROR("SDL_SetWindowRelativeMouseMode failed: %s", SDL_GetError());
		return;
	}

	m_mouseCaptured = captured;
}

void ModuleInput::SetScriptInputEnabled(bool enabled)
{
	if (enabled == m_scriptInputEnabled)
		return;

	if (!enabled)
	{
		// Falling edge — drop a live capture so the cursor reappears in the rest of the
		// editor. Remember whether we did so we can restore on the rising edge.
		if (m_mouseCaptured)
		{
			SetMouseCaptured(false);
			m_captureSuspendedByGate = true;
		}
	}
	else
	{
		// Rising edge — clamp leftover deltas accumulated while scripts were paused so
		// the camera doesn't snap by the full off-panel mouse travel on the first frame.
		mouseXMotion = 0;
		mouseYMotion = 0;

		if (m_captureSuspendedByGate)
		{
			SetMouseCaptured(true);
			m_captureSuspendedByGate = false;
		}
	}

	m_scriptInputEnabled = enabled;
}


int32_t ModuleInput::GetMouseX() const
{
	return mouseX;
}

int32_t ModuleInput::GetMouseY() const
{
	return mouseY;
}

int32_t ModuleInput::GetMouseZ() const
{
	return mouseZ;
}

int32_t ModuleInput::GetMouseXMotion() const
{
	return mouseXMotion;
}

int32_t ModuleInput::GetMouseYMotion() const
{
	return mouseYMotion;
}
