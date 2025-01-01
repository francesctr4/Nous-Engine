#pragma once

#include "IEditorWindow.inl"

class Resources : public IEditorWindow
{
public:

    explicit Resources(const char* title, bool start_open = true);

    void Init() override;
    void Draw() override;
};