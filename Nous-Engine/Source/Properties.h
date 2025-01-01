#pragma once

#include "IEditorWindow.inl"

class Properties : public IEditorWindow 
{
public:

    explicit Properties(const char* title, bool start_open = true);

    void Init() override;
    void Draw() override;
};