#include "ModuleWindow.h"
#include "Logger.h"

#include "MemoryManager.h"

ModuleWindow::ModuleWindow(Application* app, std::string name, bool start_enabled) : Module(app, name, start_enabled)
{
    NOUS_TRACE("%s()", __FUNCTION__);

    window = nullptr;
}

ModuleWindow::~ModuleWindow()
{
    NOUS_TRACE("%s()", __FUNCTION__);
}

bool ModuleWindow::Awake()
{
    NOUS_TRACE("%s()", __FUNCTION__);

    bool ret = true;

    // Initialize SDL

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {

        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());

        ret = false;
    }

    // Create a window

    Uint32 flags = SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;

    window = SDL_CreateWindow(TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, flags);

    if (window == NULL) {

        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());

        ret = false;
    }

    return ret;
}

bool ModuleWindow::Start()
{
    NOUS_TRACE("%s()", __FUNCTION__);
    return true;
}

bool ModuleWindow::CleanUp()
{
    NOUS_TRACE("%s()", __FUNCTION__);

    if (window != nullptr)
    {
        SDL_DestroyWindow(window);
        window = nullptr;
    }

    SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    SDL_Quit();

	return true;
}

void ModuleWindow::SetTitle(const char* title)
{
    SDL_SetWindowTitle(window, title);
}

SDL_Window* GetSDLWindowData()
{
    return External->window->window;
}

void GetFramebufferSize(int32* width, int32* height)
{
    SDL_Vulkan_GetDrawableSize(GetSDLWindowData(), width, height);
}
