#pragma once

#include "IEditorWindow.inl"

class Threads : public IEditorWindow
{
public:

    explicit Threads(const char* title, bool start_open = true);

    void Init() override;
    void Draw() override;
};