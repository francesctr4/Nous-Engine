#ifndef IEDITORWINDOW_INL
#define IEDITORWINDOW_INL

// Editor Window Interface
#include "Editor/EditorContext.h"

class IEditorWindow
{
public:

    explicit IEditorWindow(const char* title, EditorContext* context, bool* p_open = nullptr, const bool start_open = true)
        : editorContext(context), title(title), internal_open(start_open), p_open(p_open ? p_open : &internal_open) {}

    virtual ~IEditorWindow() = default;

    virtual void Init() = 0;
    virtual void Draw() = 0;

    bool IsOpen() const { return *p_open; }
    void Open() const { *p_open = true; }
    void Close() const { *p_open = false; }

    const char* GetTitle() const { return title; }
    const EditorContext* GetContext() const { return editorContext; }

protected:

    EditorContext* editorContext;

    const char* title;
    bool internal_open; // Internal state if `p_open` isn't provided
    bool* p_open;       // Pointer to the open/close state
};

#endif // IEDITORWINDOW_INL