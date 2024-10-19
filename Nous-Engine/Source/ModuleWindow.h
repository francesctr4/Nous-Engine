#pragma once

#include "Module.h"

#include "SDL2.h"

class ModuleWindow : public Module 
{
public:

	// Constructor
	ModuleWindow(Application* app, std::string name, bool start_enabled = true);

	// Destructor
	virtual ~ModuleWindow();

	bool Awake() override;
	bool Start() override;
	bool CleanUp() override;

	// ---------------------------------------- \\

	SDL_Window* window;

};