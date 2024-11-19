#pragma once

#include "Module.h"
#include "Globals.h"

#define MAX_KEYBOARD_KEYS 300
#define MAX_MOUSE_BUTTONS 5

enum class KeyState
{
	IDLE,
	DOWN,
	REPEAT,
	UP
};

class ModuleInput : public Module 
{
public: 

	ModuleInput(Application* app, std::string name, bool start_enabled = true);
	virtual ~ModuleInput();

	bool Awake() override;
	bool Start() override;
	UpdateStatus PreUpdate(float dt) override;
	bool CleanUp() override;

	void ReceiveEvent(const Event& event) override;

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

	int32 mouseX;
	int32 mouseY;
	int32 mouseZ;

	int32 mouseXMotion;
	int32 mouseYMotion;

};