#pragma once

#include <EditorCore/EditorExport.h>

#include <span>
#include <string_view>

class Component;
class GameObject;
class Scene;
class EditorContext;
class ModuleResourceManager;
class RendererFrontend;
class ScriptManager;

// Everything any component drawer might need, assembled once per frame by
// InspectorWindow::DrawContent. Individual drawers read only the fields they use.
struct InspectorCtx
{
    GameObject*            go            = nullptr;
    EditorContext*         editor        = nullptr;
    Scene*                 scene         = nullptr;
    ModuleResourceManager* rm            = nullptr;
    RendererFrontend*      renderer      = nullptr;
    ScriptManager*         scriptManager = nullptr;
};

using DrawFn = void(*)(const InspectorCtx&, Component*);

// One row per inspectable component type. `typeName` must equal Component::GetType().
struct ComponentUI
{
    std::string_view typeName;
    const char*      displayName;
    bool             userAddable;  // false => never shown in the Add-Component menu
    DrawFn           draw;
};

// Lookup by Component::GetType(); nullptr if the type has no inspector UI.
NOUS_EDITOR_API const ComponentUI* FindComponentUI(std::string_view typeName);

// The full table (for the Add-Component menu and tests).
NOUS_EDITOR_API std::span<const ComponentUI> ComponentUITable();
