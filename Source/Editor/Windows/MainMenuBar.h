#ifndef MAINMENUBAR_H
#define MAINMENUBAR_H

#include <Editor/IEditorWindow.inl>

class MainMenuBar : public IEditorWindow
{
public:

    explicit MainMenuBar(const char* title, bool start_open = true);

    void Init() override;
    void Draw() override;
};

#endif // MAINMENUBAR_H