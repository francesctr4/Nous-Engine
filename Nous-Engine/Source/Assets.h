#pragma once

#include "IEditorWindow.inl"

class Assets : public IEditorWindow
{
public:

    explicit Assets(const char* title, bool start_open = true);

    void Draw() override;
};