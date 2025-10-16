#ifndef NOUS_ENGINE_INSPECTORWINDOW_H
#define NOUS_ENGINE_INSPECTORWINDOW_H

#include "Editor/UI/IEditorWindow.inl"

class InspectorWindow : public IEditorWindow
{
public:

    explicit InspectorWindow(const char* title, bool start_open = true);

    void Init() override;
    void Draw() override;

};

#endif //NOUS_ENGINE_INSPECTORWINDOW_H
