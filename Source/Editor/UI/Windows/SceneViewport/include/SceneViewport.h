#ifndef SCENEVIEWPORT_H
#define SCENEVIEWPORT_H

#include "Editor/UI/IEditorWindow.inl"
#include "Editor/EditorExport.h"
#include <string>

struct VulkanContext;

class SceneViewport : public IEditorWindow
{
public:

    NOUS_EDITOR_API explicit SceneViewport(const char* title, bool start_open = true);

    NOUS_EDITOR_API void Init() override;
    NOUS_EDITOR_API void Draw() override;

    static void CreateSceneViewportDescriptorSets();
    static void DestroySceneViewportDescriptorSets();

private:

    bool IsValidASCII(const std::string& str);

};

#endif // SCENEVIEWPORT_H