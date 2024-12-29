#pragma once

#include "Globals.h"
#include "ImGui.h"
#include <Windows.h>

// Editor Window Interface

class IEditorWindow 
{
public:

    explicit IEditorWindow(const char* title, bool* p_open = nullptr, bool start_open = true)
        : title(title), internal_open(start_open), p_open(p_open ? p_open : &internal_open) {}

    virtual ~IEditorWindow() = default;

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

static void RequestBrowser(const char* url)
{
    HINSTANCE result = ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOWNORMAL);
}