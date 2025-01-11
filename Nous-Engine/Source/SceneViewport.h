#pragma once

#include "IEditorWindow.inl"

class SceneViewport : public IEditorWindow
{
public:

    explicit SceneViewport(const char* title, bool start_open = true);

    void Init() override;
    void Draw() override;
};