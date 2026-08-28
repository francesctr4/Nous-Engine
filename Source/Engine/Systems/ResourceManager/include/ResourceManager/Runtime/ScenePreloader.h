#pragma once

#include <EngineCore/EngineExport.h>

#include <future>
#include <string>
#include <vector>

class IResourceLoader;
namespace nous::engine::multithreading { class NOUS_JobSystem; }

// Parses a .nous scene file, collects all CMesh resource requests,
// and submits one parallel Deserialize job per unique (assetPath, submeshIndex) pair.
// Extracted from ModuleResourceManager — all parson and async dispatch logic lives here.
class ScenePreloader
{
public:
    NOUS_ENGINE_API explicit ScenePreloader(IResourceLoader* resourceLoader);

    // Returns futures for each submitted job — caller waits on all of them
    // before calling Scene::Deserialize() so resource lookups hit the cache.
    NOUS_ENGINE_API std::vector<std::future<void>> PreloadSceneResourcesAsync(
        nous::engine::multithreading::NOUS_JobSystem* jobSystem,
        const std::string& sceneFilePath);

private:
    IResourceLoader* m_resourceLoader = nullptr;
};
