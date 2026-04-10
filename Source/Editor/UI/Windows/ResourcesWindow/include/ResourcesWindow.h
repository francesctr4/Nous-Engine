#ifndef RESOURCESWINDOW_H
#define RESOURCESWINDOW_H

#include "Editor/UI/IEditorWindow.inl"
#include <cstdint>

enum class ResourceType : int8_t;
class Resource;
struct ImVec4;

class Resources : public IEditorWindow
{
public:

    explicit Resources(const char* title, ::EditorContext* context, bool start_open = true);

    void Init() override;
    void Draw() override;

private:

    void AlignHeadersToCenter();

    void ChooseTextColor(const ResourceType& type, ImVec4& color);
    void DisplayResource(const Resource* resource, const ImVec4& textColor);
};

#endif // RESOURCESWINDOW_H