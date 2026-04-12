#pragma once

#include "Engine/EngineExport.h"

#include <future>
#include <string>
#include <vector>

class IResourceLoader;
namespace NOUS_Multithreading { class NOUS_JobSystem; }

// Parses a .nous scene file, collects all CMesh resource requests,
// and submits one parallel Deserialize job per unique (assetPath, submeshIndex) pair.
// Extracted from ModuleResourceManager — all parson and async dispatch logic lives here.
class SceneResourcePreloader
{
public:
    NOUS_ENGINE_API explicit SceneResourcePreloader(IResourceLoader* resourceLoader);

    // Returns futures for each submitted job — caller waits on all of them
    // before calling Scene::Deserialize() so resource lookups hit the cache.
    NOUS_ENGINE_API std::vector<std::future<void>> PreloadSceneResourcesAsync(
        NOUS_Multithreading::NOUS_JobSystem* jobSystem,
        const std::string& sceneFilePath);

private:
    IResourceLoader* m_resourceLoader = nullptr;
};
