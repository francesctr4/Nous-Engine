#ifndef MULTITHREADINGWINDOW_H
#define MULTITHREADINGWINDOW_H

#include "Editor/UI/IEditorWindow.inl"

class Multithreading : public IEditorWindow
{
public:

    explicit Multithreading(const char* title, ::EditorContext* context, bool start_open = true);

    void Init() override;
    void Draw() override;
};

#endif // MULTITHREADINGWINDOW_H