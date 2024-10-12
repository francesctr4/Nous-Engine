#pragma once

#include "Module.h"
#include "SDL2.h"
#include "Globals.h"

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

private:

	KeyState* keyboard;
	KeyState mouse_buttons[MAX_MOUSE_BUTTONS];

	int mouse_x;
	int mouse_y;
	int mouse_z;

	int mouse_x_motion;
	int mouse_y_motion;

};