#ifndef MODULEINPUT_H
#define MODULEINPUT_H

#include <ModuleBase/Module.h>
#include <EngineCore/Globals.h>
#include <EventSystem/IEventListener.h>
#include <EngineCore/IInputReader.h>
#include <Scripting/iScriptInput.h>

constexpr int32 MAX_KEYBOARD_KEYS = 300;
constexpr int32 MAX_MOUSE_BUTTONS = 5;

// IScriptInput already derives IInputReader, so ModuleCamera3D's narrower
// IInputReader* view of this module keeps working unchanged.
class ModuleInput : public Module, public IEventListener, public IScriptInput
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

	int32 GetMouseX() const override;
	int32 GetMouseY() const override;
	int32 GetMouseZ() const override;

	int32 GetMouseXMotion() const override;
	int32 GetMouseYMotion() const override;

	// Relative mouse mode: hides the cursor and reports unbounded motion deltas
	// (the OS no longer clamps the cursor to the screen). Use for FPS-style camera input.
	// In editor mode the OS cursor stays visible (ImGui needs it), but the logical capture
	// state is still tracked so script code sees identical behavior in editor and game.
	NOUS_ENGINE_API void SetMouseCaptured(bool captured) override;
	NOUS_ENGINE_API bool IsMouseCaptured() const override { return m_mouseCaptured; }

	// Set once at Application construction. Read by SetMouseCaptured to gate capture in editor.
	NOUS_ENGINE_API void SetGameMode(bool gameMode) { m_gameMode = gameMode; }

	// Gates the input that script bindings see. In GameApp this stays true forever; in the
	// editor the GameViewport pushes ImGui::IsWindowFocused() every frame so scripts only
	// react while the game panel is the focused window. When transitioning to disabled the
	// logical mouse capture is dropped (and remembered) so the cursor reappears in the rest
	// of the editor; it is restored on the next enabled transition.
	NOUS_ENGINE_API void SetScriptInputEnabled(bool enabled);
	NOUS_ENGINE_API bool IsScriptInputEnabled() const override { return m_scriptInputEnabled; }

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

	bool m_scriptInputEnabled       = true;  // gates the input that script bindings observe
	bool m_captureSuspendedByGate   = false; // true if SetScriptInputEnabled(false) dropped a live capture

};

#endif // MODULEINPUT_H