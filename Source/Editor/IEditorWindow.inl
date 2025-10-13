#ifndef IEDITORWINDOW_INL
#define IEDITORWINDOW_INL

#include "Engine/Core/Globals.h"
#include "Engine/Renderer/RendererTypes.h"
#include "Engine/Core/UpdateStatus.h"
#include <vector>
#include <memory>

// Editor Window Interface

class IEditorWindow 
{
public:

    explicit IEditorWindow(const char* title, bool* p_open = nullptr, bool start_open = true)
        : title(title), internal_open(start_open), p_open(p_open ? p_open : &internal_open) {}

    virtual ~IEditorWindow() = default;

    virtual void Init() = 0;
    virtual void Draw() = 0;

    bool IsOpen() const { return *p_open; }
    void Open() { *p_open = true; }
    void Close() { *p_open = false; }

    const char* GetTitle() { return title; }

protected:

    const char* title;

    bool internal_open; // Internal state if `p_open` isn't provided
    bool* p_open;       // Pointer to the open/close state
};

static IEditorWindow* FindWindowByTitle(const std::vector<std::unique_ptr<IEditorWindow>>& windows, const char* title) {
    auto it = std::find_if(windows.begin(), windows.end(), [&title](const std::unique_ptr<IEditorWindow>& window) {
        return (strcmp(window->GetTitle(), title) == 0);
        });
    return it != windows.end() ? it->get() : nullptr;
}

static void ToggleWindowState(std::vector<std::unique_ptr<IEditorWindow>>& windows, const char* title)
{
    IEditorWindow* window_ptr = FindWindowByTitle(windows, title);
    if (window_ptr && !(window_ptr)->IsOpen()) 
    {
        (window_ptr)->Open();
    }
}

#ifdef _WIN32
#include <Windows.h>
#include <shellapi.h>
#elif defined(__APPLE__)
#include <cstdlib>
    #include <unistd.h>
#elif defined(__linux__)
    #include <cstdlib>
    #include <unistd.h>
    #include <sys/wait.h>
#endif

static bool RequestBrowser(const char* url)
{
#ifdef _WIN32
    HINSTANCE result = ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOWNORMAL);
    return (INT_PTR)result > 32; // ShellExecute devuelve > 32 si es exitoso
#elif defined(__APPLE__)
    pid_t pid = fork();
    if (pid == 0) {
        execl("/usr/bin/open", "open", url, nullptr);
        exit(1);
    }
    return pid > 0;
#elif defined(__linux__)
    pid_t pid = fork();
    if (pid == 0) {
        execl("/usr/bin/xdg-open", "xdg-open", url, nullptr);
        exit(1);
    }
    return pid > 0;
#else
    return false; // Plataforma no soportada
#endif
}

#endif // IEDITORWINDOW_INL