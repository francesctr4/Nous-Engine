#ifndef GAMEVIEWPORT_H
#define GAMEVIEWPORT_H

#include "Editor/UI/IEditorWindow.inl"
#include "Editor/EditorExport.h"

class GameViewport : public IEditorWindow
{
public:

    NOUS_EDITOR_API explicit GameViewport(const char* title, EditorContext* context, bool start_open = true);

    NOUS_EDITOR_API void Init() override;
    NOUS_EDITOR_API void Draw() override;

    static void CreateGameViewportDescriptorSets();
    static void DestroyGameViewportDescriptorSets();

};

#endif // GAMEVIEWPORT_H