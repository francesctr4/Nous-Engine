#pragma once

#include <ModuleBase/Module.h>
#include <EventSystem/IEventListener.h>
#include "Engine/Core/Globals.h"
#include "Engine/EngineExport.h"
#include <Renderer/iRenderWindow.h>

struct SDL_Window;

class ModuleWindow : public Module, public IEventListener, public IRenderWindow
{
public:

	// Constructor
	ModuleWindow(EventSystem* eventSystem, nous::engine::multithreading::NOUS_JobSystem* jobSystem);

	// Destructor
	~ModuleWindow() override;

	bool Awake() override;
	bool Start() override;
	bool CleanUp() override;

	void OnEvent(const Event& event) override;

	// ----------------------------------------

	void SetTitle(const char* title) const;

	void Maximize() const;
	NOUS_ENGINE_API void SetFullscreen(bool fullscreen) const;

	NOUS_ENGINE_API void SetMinimized(bool value);
	[[nodiscard]] NOUS_ENGINE_API bool IsMinimized() const;

    // ---- IRenderWindow -----------------------------------------------------
    [[nodiscard]] NOUS_ENGINE_API SDL_Window* GetSDL_Window() const override;
    void GetFramebufferSize(int32* width, int32* height) const override;

private:

	SDL_Window* m_window;
	bool m_isMinimized;

};