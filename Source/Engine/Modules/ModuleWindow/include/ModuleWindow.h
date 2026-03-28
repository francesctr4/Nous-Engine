#ifndef MODULEWINDOW_H
#define MODULEWINDOW_H

#include "Engine/Modules/Module.h"
#include "Engine/Core/EventSystem/IEventListener.h"
#include "Engine/Core/Globals.h"
#include "Engine/EngineExport.h"

struct SDL_Window;

class ModuleWindow : public Module, public IEventListener
{
public:

	// Constructor
	ModuleWindow(EventSystem* eventSystem, NOUS_Multithreading::NOUS_JobSystem* jobSystem);

	// Destructor
	virtual ~ModuleWindow();

	bool Awake() override;
	bool Start() override;
	bool CleanUp() override;

	void OnEvent(const Event& event) override;

	// ---------------------------------------- \\

	void SetTitle(const char* title);
	void Maximize() const;
	NOUS_ENGINE_API void SetFullscreen(bool fullscreen);
	NOUS_ENGINE_API void SetMinimized(bool value);
	NOUS_ENGINE_API bool IsMinimized() const;

    NOUS_ENGINE_API SDL_Window* GetSDL_Window();
    void GetFramebufferSize(int32* width, int32* height);

private:

	SDL_Window* window;
	bool mIsMinimized = false;

};

#endif // MODULEWINDOW_H