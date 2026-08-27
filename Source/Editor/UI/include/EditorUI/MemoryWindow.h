#ifndef MEMORYWINDOW_H
#define MEMORYWINDOW_H

#include <EditorUI/IEditorWindow.h>

#include <array>

class MemoryWindow : public IEditorWindow
{
public:

    explicit MemoryWindow(const char* title, ::EditorContext* context, bool start_open = true);

    void Update() override;
    void DrawContent() override;

private:

    struct MemoryUsageHistory
    {
        static constexpr int MaxSamples = 300; // ~5 seconds at 60 FPS
        std::array<float, MaxSamples> values = {};
        int currentIndex = 0;

        void AddValue(float value)
        {
            values[currentIndex] = value;
            currentIndex = (currentIndex + 1) % MaxSamples;
        }
    };

    MemoryUsageHistory history;
};

#endif // MEMORYWINDOW_H