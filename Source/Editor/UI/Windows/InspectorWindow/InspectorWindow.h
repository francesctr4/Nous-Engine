#pragma once

#include "Editor/UI/IEditorWindow.h"
#include "Engine/Scripting/Internal/IScript.inl"
#include "Engine/Systems/ECS/Component/CMaterial/include/CMaterial.h"
#include "Engine/Systems/ECS/Scene/include/Scene.h"
#include "Engine/Systems/ResourceManager/ResourceTypes/Resource/ResourceShader/include/ResourceShader.h"
#include "Engine/Systems/ResourceManager/ResourceTypes/Resource/ResourceTexture/include/ResourceTexture.h"
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

    void DrawTransformComponent(CTransform* ctransform) const;
    void DrawMeshComponent(const CMesh* cmesh) const;
    void DrawCameraComponent(CCamera* ccamera) const;
    void DrawLightComponent(CLight* clight) const;
    void DrawMaterialComponent(const GameObject* go, CMaterial* cmaterial,
        ModuleResourceManager* rm, RendererFrontend* rendererFrontend) const;
    void DrawScriptComponent(Scene* scene, const ScriptManager* scriptManager, CScript* cscript) const;

    void DrawAddComponentSection(GameObject* go) const;

    std::string m_nameBuffer;
    uint32_t m_lastSelectedID = UINT32_MAX;

};
