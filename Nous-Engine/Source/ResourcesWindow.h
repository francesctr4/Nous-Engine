#pragma once

#include "IEditorWindow.inl"

enum class ResourceType;
class Resource;

class Resources : public IEditorWindow
{
public:

    explicit Resources(const char* title, bool start_open = true);

    void Init() override;
    void Draw() override;

private:

    void AlignHeadersToCenter();

    void ChooseTextColor(const ResourceType& type, ImVec4& color);
    void DisplayResource(const Resource* resource, const ImVec4& textColor);
};