#ifndef NOUS_ENGINE_EDITORCONTEXT_H
#define NOUS_ENGINE_EDITORCONTEXT_H

#include <cstddef>
#include <string>

class ImFont;

// Forward declarations
class ModuleScene;
class ModuleCamera3D;
class ModuleInput;
class ModuleResourceManager;
class RendererFrontend;
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

    [[nodiscard]] virtual std::string GetAssetsBrowserDirectory() const = 0;

    // Called when a .glsl file is moved in the AssetsBrowser — keeps the hot-reload watcher in sync.
    virtual void UpdateShaderWatcherPath(const std::string& oldPath, const std::string& newPath) = 0;

};

#endif //NOUS_ENGINE_EDITORCONTEXT_H
