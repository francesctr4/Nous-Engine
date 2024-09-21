#include "ModuleWindow.h"

ModuleWindow::ModuleWindow(Application* app, std::string name, bool start_enabled) : Module(app, name, start_enabled)
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

bool ModuleWindow::CleanUp()
{
	return true;
}

bool ModuleWindow::InitSDLWindow()
{
	return true;
}
