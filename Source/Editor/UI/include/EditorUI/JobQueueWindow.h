#ifndef JOBQUEUEWINDOW_H
#define JOBQUEUEWINDOW_H

#include <EditorUI/IEditorWindow.h>

class JobQueue : public IEditorWindow
{
public:

    explicit JobQueue(const char* title, EditorContext* context, bool start_open = true);

    void DrawContent() override;

private:

    void DrawQueue() const;

    // Synthetic job submission, so the queue above has something to show. Lives here
    // rather than on a global hotkey: it is only meaningful next to this readout.
    void DrawTests() const;
};

#endif // JOBQUEUEWINDOW_H