#pragma once

#include "Editor/UI/IEditorWindow.h"
#include "Engine/Scripting/Internal/IScript.inl"
#include <ECS/Component/Types/CMaterial.h>
#include <ECS/Scene/Scene.h>
#include "Engine/Systems/ResourceManager/Types/ResourceShader/include/ResourceShader.h"
#include "Engine/Systems/ResourceManager/Types/ResourceTexture/include/ResourceTexture.h"
#include <cstdint>
#include <string>

class ScriptManager;
class CScript;
class CLight;
class CCamera;
class CTransform;
class CMesh;
class GameObject;

class InspectorWindow : public IEditorWindow
{
public:

    explicit InspectorWindow(const char* title, EditorContext* context, bool start_open = true);

protected:

    void DrawContent() override;

private:

    void DrawGameObjectHeader(GameObject* go);

    void DrawAddComponentSection(GameObject* go) const;

    std::string m_nameBuffer;
    uint32_t m_lastSelectedID = UINT32_MAX;

};
