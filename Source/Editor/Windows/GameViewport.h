#ifndef GAMEVIEWPORT_H
#define GAMEVIEWPORT_H

#include <Editor/IEditorWindow.inl>

class GameViewport : public IEditorWindow
{
public:

    explicit GameViewport(const char* title, bool start_open = true);

    void Init() override;
    void Draw() override;

};

#endif // GAMEVIEWPORT_H