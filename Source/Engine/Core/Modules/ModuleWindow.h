#ifndef MODULEWINDOW_H
#define MODULEWINDOW_H

#include <Engine/Core/Module.h>
#include "Engine/Systems/Event System/IEventListener.h"

struct SDL_Window;

class ModuleWindow : public Module, public IEventListener
{
public:

	// Constructor
	ModuleWindow(Application* app);

	// Destructor
	virtual ~ModuleWindow();

	bool Awake() override;
	bool Start() override;
	bool CleanUp() override;

	void OnEvent(const Event& event) override;

	// ---------------------------------------- \\

	void SetTitle(const char* title);

	SDL_Window* window;

};

#endif // MODULEWINDOW_H