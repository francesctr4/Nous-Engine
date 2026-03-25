#include "Engine/Modules/ModuleWindow/include/ModuleWindow.h"

#include "SDL3/SDL.h"
#include "SDL3/SDL_vulkan.h"

#include "Engine/Core/Application.h"
#include "Engine/Core/Logger/Logger.h"

ModuleWindow::ModuleWindow(Application* app) : Module(app)
{
    window = nullptr;
}

ModuleWindow::~ModuleWindow()
{

}

bool ModuleWindow::Awake()
{
    bool ret = true;

    // Initialize SDL
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        NOUS_ERROR("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        ret = false;
    }

    // Create window only if Vulkan loaded successfully
    if (ret) {
        Uint32 flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;

        window = SDL_CreateWindow(TITLE, WINDOW_WIDTH, WINDOW_HEIGHT, flags);

        if (window == nullptr) {
            NOUS_ERROR("Window could not be created! SDL_Error: %s\n", SDL_GetError());
            ret = false;
        }
        else {
            SDL_SetWindowPosition(window,
                                  SDL_WINDOWPOS_CENTERED,
                                  SDL_WINDOWPOS_CENTERED);
        }
    }

    return ret;
}

bool ModuleWindow::Start()
{
    SDL_MaximizeWindow(window);

    return true;
}

bool ModuleWindow::CleanUp()
{
    if (window != nullptr)
    {
        SDL_DestroyWindow(window);
        window = nullptr;
    }

    SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    SDL_Quit();

	return true;
}

void ModuleWindow::OnEvent(const Event &event)
{
    switch (event.type)
    {
        default:
            break;
    }
}

void ModuleWindow::SetTitle(const char* title)
{
    SDL_SetWindowTitle(window, title);
}

void ModuleWindow::SetFullscreen(bool fullscreen)
{
    SDL_SetWindowFullscreen(window, fullscreen);
}

SDL_Window* ModuleWindow::GetSDL_Window()
{
    return window;
}

void ModuleWindow::GetFramebufferSize(int32* width, int32* height)
{
    SDL_GetWindowSizeInPixels(window, width, height);
}
