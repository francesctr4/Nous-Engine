#ifndef SCENEVIEWPORT_H
#define SCENEVIEWPORT_H

#include "Editor/IEditorWindow.inl"

class SceneViewport : public IEditorWindow
{
public:

    explicit SceneViewport(const char* title, bool start_open = true);

    void Init() override;
    void Draw() override;

private:

    bool IsValidASCII(const std::string& str);

};

#endif // SCENEVIEWPORT_H