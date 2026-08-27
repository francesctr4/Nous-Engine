#ifndef JOBQUEUEWINDOW_H
#define JOBQUEUEWINDOW_H

#include <EditorUI/IEditorWindow.h>

class JobQueue : public IEditorWindow
{
public:

    explicit JobQueue(const char* title, EditorContext* context, bool start_open = true);

    void DrawContent() override;
};

#endif // JOBQUEUEWINDOW_H