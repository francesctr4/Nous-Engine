#ifndef MODULEINPUT_H
#define MODULEINPUT_H

#include "Engine/Modules/Module.h"
#include "Engine/Core/Globals.h"
#include "Engine/Core/EventSystem/IEventListener.h"
#include "Engine/Core/Input/IInputReader.h"

constexpr int32 MAX_KEYBOARD_KEYS = 300;
constexpr int32 MAX_MOUSE_BUTTONS = 5;

class ModuleInput : public Module, public IEventListener, public IInputReader
{
public:

	ModuleInput(EventSystem* eventSystem, nous::engine::multithreading::NOUS_JobSystem* jobSystem);
	~ModuleInput() override;

	bool Awake() override;
	bool Start() override;
	UpdateStatus PreUpdate(float dt) override;
	bool CleanUp() override;

	void OnEvent(const Event& event) override;

	NOUS_ENGINE_API KeyState GetKey(int id) const override;
	NOUS_ENGINE_API KeyState GetMouseButton(int id) const override;

	NOUS_ENGINE_API void SetImGuiCaptureKeyboard(bool captured);

	int32 GetMouseX() const;
	int32 GetMouseY() const;
	int32 GetMouseZ() const override;

	int32 GetMouseXMotion() const override;
	int32 GetMouseYMotion() const override;

	// Relative mouse mode: hides the cursor and reports unbounded motion deltas
	// (the OS no longer clamps the cursor to the screen). Use for FPS-style camera input.
	// In editor mode the OS cursor stays visible (ImGui needs it), but the logical capture
	// state is still tracked so script code sees identical behavior in editor and game.
	NOUS_ENGINE_API void SetMouseCaptured(bool captured);
	NOUS_ENGINE_API bool IsMouseCaptured() const { return m_mouseCaptured; }

	// Set once at Application construction. Read by SetMouseCaptured to gate capture in editor.
	NOUS_ENGINE_API void SetGameMode(bool gameMode) { m_gameMode = gameMode; }

private:

	KeyState* keyboard;
	KeyState mouseButtons[MAX_MOUSE_BUTTONS];

	float mouseX;
	float mouseY;
	int32 mouseZ;

	int32 mouseXMotion;
	int32 mouseYMotion;

	int32 m_lastWindowWidth  = 0;
	int32 m_lastWindowHeight = 0;

	bool m_imguiCaptureKeyboard = false;

	bool m_mouseCaptured = false;
	bool m_gameMode      = false;  // true in GameApp.exe; gates SetMouseCaptured(true)

};

#endif // MODULEINPUT_H