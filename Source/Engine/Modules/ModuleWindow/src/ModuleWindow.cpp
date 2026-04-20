#include "Engine/Modules/ModuleWindow/include/ModuleWindow.h"

#include "Engine/Core/Logger/Logger.h"
#include "Engine/Core/EventSystem/EventSystem.h"
#include "Engine/Core/EventSystem/Event/include/Event.h"

#include "SDL3/SDL.h"
#include "SDL3/SDL_vulkan.h"

ModuleWindow::ModuleWindow(EventSystem* eventSystem, nous::engine::multithreading::NOUS_JobSystem* jobSystem)
    : Module(eventSystem, jobSystem), m_window(nullptr), m_isMinimized(false) {}

ModuleWindow::~ModuleWindow() = default;

bool ModuleWindow::Awake()
{
    bool ret = true;

    eventSystem->Subscribe(EventType::WINDOW_MINIMIZED, this);

    // Initialize SDL
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
    {
        NOUS_ERROR("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        ret = false;
    }

    // Create window only if Vulkan loaded successfully
    if (ret)
    {
        Uint32 flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;

        m_window = SDL_CreateWindow(TITLE, WINDOW_WIDTH, WINDOW_HEIGHT, flags);

        if (m_window == nullptr)
        {
            NOUS_ERROR("Window could not be created! SDL_Error: %s\n", SDL_GetError());
            ret = false;
        }
        else
        {
            SDL_SetWindowPosition(m_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        }
    }

    return ret;
}

bool ModuleWindow::Start()
{
    return true;
}

void ModuleWindow::Maximize() const
{
    SDL_MaximizeWindow(m_window);
}

bool ModuleWindow::CleanUp()
{
    if (m_window != nullptr)
    {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }

    SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    SDL_Quit();

	return true;
}

void ModuleWindow::OnEvent(const Event &event)
{
    switch (event.type)
    {
        case EventType::WINDOW_MINIMIZED:
            {
                m_isMinimized = event.ctx.u8[0];
                break;
            }

        default:
            {
                break;
            }
    }
}

void ModuleWindow::SetTitle(const char* title) const
{
    SDL_SetWindowTitle(m_window, title);
}

void ModuleWindow::SetFullscreen(bool fullscreen) const
{
    SDL_SetWindowFullscreen(m_window, fullscreen);
}

void ModuleWindow::SetMinimized(bool value)
{
    m_isMinimized = value;
    eventSystem->Broadcast(Event(EventType::WINDOW_MINIMIZED, SendContext(value)));
}

bool ModuleWindow::IsMinimized() const
{
    return m_isMinimized;
}

SDL_Window* ModuleWindow::GetSDL_Window() const
{
    return m_window;
}

void ModuleWindow::GetFramebufferSize(int32* width, int32* height) const
{
    SDL_GetWindowSizeInPixels(m_window, width, height);
}
