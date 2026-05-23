#ifndef MULTITHREADINGWINDOW_H
#define MULTITHREADINGWINDOW_H

#include "Editor/UI/IEditorWindow.h"

class Multithreading : public IEditorWindow
{
public:

    explicit Multithreading(const char* title, ::EditorContext* context, bool start_open = true);

    void Update() override;
    void DrawContent() override;
};

#endif // MULTITHREADINGWINDOW_H