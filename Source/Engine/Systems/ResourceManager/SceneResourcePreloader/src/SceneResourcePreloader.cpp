#include "Engine/Systems/ResourceManager/SceneResourcePreloader/include/SceneResourcePreloader.h"

#include "Engine/Systems/ResourceManager/IResourceLoader.h"
#include "Engine/Core/Logger/Logger.h"
#include "Engine/Core/Globals.h"
#include "Engine/Systems/ResourceManager/Resource/Resource.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceMesh/include/ResourceMesh.h"
#include "Engine/NOUS_Multithreading/NOUS_JobSystem/include/NOUS_JobSystem.h"

#include "Engine/Utils/Serialization/JsonFile/JsonFile.h"
#include "Engine/Utils/Serialization/JsonFile/JsonArray.h"
#include <filesystem>
#include <map>
#include <ranges>
#include <utility>

constexpr auto CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_CORE_MODULE_RESOURCEMANAGER;

namespace
{
    struct MeshRequest
    {
        std::string assetPath;
        std::string libraryPath;
        uint32      uid          = 0;
        int32_t     submeshIndex = -1;
    };

    // Dispatches the correct IResourceLoader load call based on the request fields.
    // Returns the loaded Resource* (refcount already bumped) or nullptr on failure.
    // Caller must call DecreaseReferenceCount() when done.
    Resource* LoadMeshRequest(IResourceLoader* loader, const MeshRequest& req)
    {
        if (req.submeshIndex >= 0)
        {
            if (!req.libraryPath.empty())
                return loader->RequestOrCreateSubMeshResourceFromLibrary(req.libraryPath, req.submeshIndex, req.assetPath, req.uid);
            return loader->RequestOrCreateSubMeshResource(req.assetPath, req.submeshIndex);
        }

        if (!req.libraryPath.empty() && req.uid != 0)
            return loader->CreateResourceFromLibrary(req.uid, ResourceType::MESH,
                std::filesystem::path(req.assetPath).filename().string(),
                req.assetPath, req.libraryPath);

        return loader->CreateResource(req.assetPath);
    }

    // Walks the GameObjects JSON array and collects every unique CMesh request.
    // Key: (assetPath, submeshIndex) — mirrors ResourceManager's internal deduplication.
    void CollectMeshRequestsFromScene(
        const JsonArray& gameObjects,
        std::map<std::pair<std::string, int32_t>, MeshRequest>& outRequests)
    {
        const int goCount = gameObjects.Count();
        for (int i = 0; i < goCount; ++i)
        {
            JsonObject goObj = gameObjects.GetObject(i);
            JsonArray  comps = goObj.GetArray("components");
            if (comps.IsEmpty()) continue;

            const int compCount = comps.Count();
            for (int j = 0; j < compCount; ++j)
            {
                JsonObject compObj = comps.GetObject(j);

                if (compObj.GetString("type") != "CMesh")
                    continue;

                const std::string assetPath = compObj.GetString("assetPath");
                if (assetPath.empty()) continue;

                MeshRequest req;
                req.assetPath    = assetPath;
                req.libraryPath  = compObj.GetString("libraryPath");
                req.uid          = static_cast<uint32>(compObj.GetDouble("resourceUID",  0.0));
                req.submeshIndex = compObj.HasKey("submeshIndex")
                                 ? static_cast<int32_t>(compObj.GetDouble("submeshIndex", 0.0))
                                 : -1;

                outRequests[{req.assetPath, req.submeshIndex}] = std::move(req);
            }
        }
    }
} // namespace

SceneResourcePreloader::SceneResourcePreloader(IResourceLoader* resourceLoader)
    : m_resourceLoader(resourceLoader)
{
}

std::vector<std::future<void>> SceneResourcePreloader::PreloadSceneResourcesAsync(
    nous::engine::multithreading::NOUS_JobSystem* jobSystem,
    const std::string& sceneFilePath)
{
    std::vector<std::future<void>> futures;

    JsonObject root = JsonFile::LoadFromFile(sceneFilePath);
    if (root.IsEmpty())
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "PreloadSceneResourcesAsync: failed to parse '%s'", sceneFilePath.c_str());
        return futures;
    }

    JsonArray gameObjects = root.GetArray("GameObjects");
    if (gameObjects.IsEmpty())
        return futures;

    std::map<std::pair<std::string, int32_t>, MeshRequest> uniqueRequests;
    CollectMeshRequestsFromScene(gameObjects, uniqueRequests);

    futures.reserve(uniqueRequests.size());

    for (const auto& req : uniqueRequests | std::views::values)
    {
        auto prom = std::make_shared<std::promise<void>>();
        futures.push_back(prom->get_future());

        jobSystem->SubmitJob([this, req, prom]
        {
            // Release the preload's reference. The resource stays in the map so
            // CMesh::Deserialize() hits the fast path, but the preload does not
            // hold an extra ref that would prevent eviction.
            if (Resource* res = LoadMeshRequest(m_resourceLoader, req))
                res->DecreaseReferenceCount();
            prom->set_value();
        }, "Preload: " + req.assetPath);
    }

    NOUS_INFO_C(CURRENT_CHANNEL,
        "PreloadSceneResourcesAsync: submitted %zu parallel jobs for '%s'",
        uniqueRequests.size(), sceneFilePath.c_str());

    return futures;
}
