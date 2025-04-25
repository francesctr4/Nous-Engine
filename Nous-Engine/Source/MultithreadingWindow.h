#pragma once

#include "IEditorWindow.inl"

class Multithreading : public IEditorWindow
{
public:

    explicit Multithreading(const char* title, bool start_open = true);

    void Init() override;
    void Draw() override;
};