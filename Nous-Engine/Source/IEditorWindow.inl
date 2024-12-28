#pragma once

#include "ImGui.h"

// Editor Window Interface

struct IEditorWindow
{
    virtual ~IEditorWindow() = default;
    virtual void Draw() = 0;
};