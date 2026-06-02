#ifndef RESOURCESWINDOW_H
#define RESOURCESWINDOW_H

#include "Editor/UI/IEditorWindow.h"
#include <cstdint>

enum class ResourceType : int8_t;
class ResourceBase;
struct ImVec4;

class Resources : public IEditorWindow
{
public:

    explicit Resources(const char* title, ::EditorContext* context, bool start_open = true);

    void DrawContent() override;

private:

    void AlignHeadersToCenter() const;
    void ChooseTextColor(const ResourceType& type, ImVec4& color) const;
    void DisplayResource(const ResourceBase* resource, const ImVec4& textColor) const;

    uint32_t previousResourceCount = 0;
};

#endif // RESOURCESWINDOW_H