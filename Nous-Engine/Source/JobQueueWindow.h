#pragma once

#include "IEditorWindow.inl"

class JobQueue : public IEditorWindow
{
public:

    explicit JobQueue(const char* title, bool start_open = true);

    void Init() override;
    void Draw() override;
};