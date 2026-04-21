#ifndef MEMORYWINDOW_H
#define MEMORYWINDOW_H

#include "Editor/UI/IEditorWindow.h"

class MemoryWindow : public IEditorWindow
{
public:

    explicit MemoryWindow(const char* title, ::EditorContext* context, bool start_open = true);

    void Update() override;
    void DrawContent() override;
};

#endif // MEMORYWINDOW_H