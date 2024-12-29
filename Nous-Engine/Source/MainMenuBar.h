#pragma once

#include "IEditorWindow.inl"

class MainMenuBar : public IEditorWindow
{
public:

    explicit MainMenuBar(const char* title, bool start_open = true);

    void Draw() override;
};