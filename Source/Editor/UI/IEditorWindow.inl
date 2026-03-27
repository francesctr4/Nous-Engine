#ifndef IEDITORWINDOW_INL
#define IEDITORWINDOW_INL

// Editor Window Interface

#include "Editor/EditorContext.h"

class IEditorWindow
{
public:

    explicit IEditorWindow(const char* title, EditorContext* context, bool* p_open = nullptr, bool start_open = true)
        : title(title), EditorContext(context), internal_open(start_open), p_open(p_open ? p_open : &internal_open) {}

    virtual ~IEditorWindow() = default;

    virtual void Init() = 0;
    virtual void Draw() = 0;

    bool IsOpen() const { return *p_open; }
    void Open() { *p_open = true; }
    void Close() { *p_open = false; }

    const char* GetTitle() const { return title; }
    const EditorContext* GetContext() const { return EditorContext; }

protected:

    EditorContext* EditorContext;

    const char* title;
    bool internal_open; // Internal state if `p_open` isn't provided
    bool* p_open;       // Pointer to the open/close state
};

//static IEditorWindow* FindWindowByTitle(const std::vector<std::unique_ptr<IEditorWindow>>& windows, const char* title) {
//    auto it = std::find_if(windows.begin(), windows.end(), [&title](const std::unique_ptr<IEditorWindow>& window) {
//        return (strcmp(window->GetTitle(), title) == 0);
//        });
//    return it != windows.end() ? it->get() : nullptr;
//}

//static void ToggleWindowState(std::vector<std::unique_ptr<IEditorWindow>>& windows, const char* title)
//{
//    IEditorWindow* window_ptr = FindWindowByTitle(windows, title);
//    if (window_ptr && !(window_ptr)->IsOpen())
//    {
//        (window_ptr)->Open();
//    }
//}

#endif // IEDITORWINDOW_INL