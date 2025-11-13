#ifndef NOUS_ENGINE_EDITORCONTEXT_H
#define NOUS_ENGINE_EDITORCONTEXT_H

class ImFont;

class EditorContext
{
public:
    virtual ImFont* GetFont(size_t index) const = 0;
};

#endif //NOUS_ENGINE_EDITORCONTEXT_H
