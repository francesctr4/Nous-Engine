#ifndef NOUS_ENGINE_INSPECTORWINDOW_H
#define NOUS_ENGINE_INSPECTORWINDOW_H

#include "Editor/UI/IEditorWindow.h"

class InspectorWindow : public IEditorWindow
{
public:

    explicit InspectorWindow(const char* title, EditorContext* context, bool start_open = true);
    
    void DrawContent() override;

};

#endif //NOUS_ENGINE_INSPECTORWINDOW_H
