#ifndef MEMORYWINDOW_H
#define MEMORYWINDOW_H

#include "Editor/UI/IEditorWindow.inl"

class MemoryWindow : public IEditorWindow
{
public:

    explicit MemoryWindow(const char* title, ::EditorContext* context, bool start_open = true);

    void Init() override;
    void Draw() override;
};

#endif // MEMORYWINDOW_H