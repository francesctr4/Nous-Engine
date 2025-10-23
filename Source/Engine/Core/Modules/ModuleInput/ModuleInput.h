#ifndef MODULEINPUT_H
#define MODULEINPUT_H

#include "Engine/Core/Modules/Module.h"
#include "Engine/Core/Globals.h"
#include "Engine/Systems/Event System/IEventListener.h"

#define MAX_KEYBOARD_KEYS 300
#define MAX_MOUSE_BUTTONS 5

enum class KeyState
{
	IDLE,
	DOWN,
	REPEAT,
	UP
};

class ModuleInput : public Module, public IEventListener
{
public: 

	ModuleInput(Application* app);
	virtual ~ModuleInput();

	bool Awake() override;
	bool Start() override;
	UpdateStatus PreUpdate(float dt) override;
	bool CleanUp() override;

	void OnEvent(const Event& event) override;

	KeyState GetKey(int id) const;
	KeyState GetMouseButton(int id) const;

	int32 GetMouseX() const;
	int32 GetMouseY() const;
	int32 GetMouseZ() const;

	int32 GetMouseXMotion() const;
	int32 GetMouseYMotion() const;

private:

	KeyState* keyboard;
	KeyState mouseButtons[MAX_MOUSE_BUTTONS];

	float mouseX;
	float mouseY;
	int32 mouseZ;

	int32 mouseXMotion;
	int32 mouseYMotion;

};

#endif // MODULEINPUT_H