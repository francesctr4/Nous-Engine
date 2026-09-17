#pragma once

#include <cstddef>
#include <functional>
#include <string>

class ImFont;
class IEditorWindow;

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

    /**
     * @brief Visits every registered editor window, in registration order.
     *
     * Exists so the Windows menu can be DERIVED from the windows that actually
     * exist rather than hand-listed beside them: a hand-listed menu is a second
     * thing to keep in sync, and it silently drifted -- it offered windows that
     * were never built and omitted every window added in the last year.
     *
     * A callback rather than a returned container, the same shape as the
     * renderer's ForEachShader / ForEachMaterial: the list lives in a NOUS_Vector
     * behind the custom allocator, and handing that type out would drag it into
     * every consumer.
     */
    virtual void ForEachEditorWindow(const std::function<void(IEditorWindow&)>& fn) const = 0;

    /**
     * @brief The registered window with this exact title, or null.
     *
     * For the cases where one window drives another -- the menu bar owning the
     * asset-pipeline actions whose state lives in the Assets Browser, say. Returns
     * the base type: the caller knows which concrete window it asked for, and must
     * null-check before casting, since a window can be absent.
     */
    [[nodiscard]] virtual IEditorWindow* GetEditorWindow(const char* title) const = 0;

    // Called when a .glsl file is moved in the AssetsBrowser — keeps the hot-reload watcher in sync.
    virtual void UpdateShaderWatcherPath(const std::string& oldPath, const std::string& newPath) = 0;

    // Called when a new .glsl file is created in the AssetsBrowser — registers it with the hot-reload watcher.
    virtual void WatchShaderFile(const std::string& path) = 0;

};
