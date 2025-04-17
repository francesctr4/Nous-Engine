#pragma once

#include "Module.h"

struct SDL_Window;

class ModuleWindow : public Module 
{
public:

	// Constructor
	ModuleWindow(Application* app);

	// Destructor
	virtual ~ModuleWindow();

	bool Awake() override;
	bool Start() override;
	bool CleanUp() override;

	// ---------------------------------------- \\

	void SetTitle(const char* title);

	SDL_Window* window;

};