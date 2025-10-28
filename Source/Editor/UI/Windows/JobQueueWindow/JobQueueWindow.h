#ifndef JOBQUEUEWINDOW_H
#define JOBQUEUEWINDOW_H

#include "Editor/UI/IEditorWindow.inl"

class JobQueue : public IEditorWindow
{
public:

    explicit JobQueue(const char* title, bool start_open = true);

    void Init() override;
    void Draw() override;
};

#endif // JOBQUEUEWINDOW_H