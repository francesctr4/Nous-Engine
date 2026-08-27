#pragma once

#include <cstddef>
#include <string>

class ImFont;

// Forward declarations
class ModuleScene;
class ModuleCamera3D;
class ModuleInput;
class ModuleResourceManager;
class RendererFrontend;
class IEditorRenderBridge;
class GameExporter;
namespace nous::engine::multithreading { class NOUS_JobSystem; }

class EditorContext
{
public:

    virtual ~EditorContext() = default;

    [[nodiscard]] virtual ImFont* GetFont(size_t index) const = 0;

    [[nodiscard]] virtual ModuleScene*                          GetScene()            const = 0;
    [[nodiscard]] virtual ModuleCamera3D*                       GetCamera()           const = 0;
    [[nodiscard]] virtual ModuleInput*                          GetInput()            const = 0;
    [[nodiscard]] virtual ModuleResourceManager*                GetResourceManager()  const = 0;
    [[nodiscard]] virtual RendererFrontend*                     GetRendererFrontend() const = 0;
    [[nodiscard]] virtual nous::engine::multithreading::NOUS_JobSystem*  GetJobSystem()        const = 0;
    [[nodiscard]] virtual GameExporter*                                  GetGameExporter()     const = 0;

    /**
     * @brief The renderer's editor-facing bridge: the ImGui-Vulkan binding, the
     *        offscreen viewport textures, and the pick framebuffer size.
     *
     * Injected, never fetched -- it replaces VulkanBackend::GetVulkanContext(),
     * which handed the whole mutable VulkanContext to any caller. Null before
     * ModuleEditor::Awake() has resolved it, and for a backend with no editor
     * support; guard it. See Engine/Renderer/iEditorRenderBridge.h.
     */
    [[nodiscard]] virtual IEditorRenderBridge*                           GetEditorRenderBridge() const = 0;

    [[nodiscard]] virtual std::string GetAssetsBrowserDirectory() const = 0;

    // Called when a .glsl file is moved in the AssetsBrowser — keeps the hot-reload watcher in sync.
    virtual void UpdateShaderWatcherPath(const std::string& oldPath, const std::string& newPath) = 0;

    // Called when a new .glsl file is created in the AssetsBrowser — registers it with the hot-reload watcher.
    virtual void WatchShaderFile(const std::string& path) = 0;

};
