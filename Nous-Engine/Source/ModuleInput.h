#pragma once

#include "Module.h"
#include "SDL2.h"
#include "Globals.h"

#define MAX_KEYBOARD_KEYS 300
#define MAX_MOUSE_BUTTONS 5
#define SDL_MAX_SINT16 32767

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

	uint32 GetMouseX() const;
	uint32 GetMouseY() const;
	uint32 GetMouseZ() const;

	uint32 GetMouseXMotion() const;
	uint32 GetMouseYMotion() const;

private:

	KeyState* keyboard;
	KeyState mouseButtons[MAX_MOUSE_BUTTONS];

	uint32 mouseX;
	uint32 mouseY;
	uint32 mouseZ;

	uint32 mouseXMotion;
	uint32 mouseYMotion;

};