#ifndef MODULEINPUT_H
#define MODULEINPUT_H

#include "Engine/Modules/Module.h"
#include "Engine/Core/Globals.h"
#include "Engine/Core/EventSystem/IEventListener.h"
#include "Engine/Core/Input/IInputReader.h"

#define MAX_KEYBOARD_KEYS 300
#define MAX_MOUSE_BUTTONS 5

class ModuleInput : public Module, public IEventListener, public IInputReader
{
public:

	ModuleInput(EventSystem* eventSystem, NOUS_Multithreading::NOUS_JobSystem* jobSystem);
	virtual ~ModuleInput();

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

};

#endif // MODULEINPUT_H