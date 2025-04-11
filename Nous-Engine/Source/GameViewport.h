#pragma once

#include "IEditorWindow.inl"

class GameViewport : public IEditorWindow
{
public:

    explicit GameViewport(const char* title, bool start_open = true);

    void Init() override;
    void Draw() override;

};