#include "Engine/Systems/ResourceManager/ScenePreloader/include/ScenePreloader.h"

#include "Engine/Systems/ResourceManager/IResourceLoader.h"
#include "Engine/Core/Logger/Logger.h"
#include "Engine/Core/Globals.h"
#include "Engine/Systems/ResourceManager/Resource/Resource.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceMesh/include/ResourceMesh.h"
#include "Engine/NOUS_Multithreading/NOUS_JobSystem/include/NOUS_JobSystem.h"

#include <filesystem>
#include <map>
#include <parson.h>
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
                return loader->RequestOrCreateSubMeshResourceFromLibrary(req.libraryPath, req.submeshIndex, req.assetPath);
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
        JSON_Array const* gameObjects,
        std::map<std::pair<std::string, int32_t>, MeshRequest>& outRequests)
    {
        const size_t goCount = json_array_get_count(gameObjects);
        for (size_t i = 0; i < goCount; ++i)
        {
            JSON_Object const* goObj = json_array_get_object(gameObjects, i);
            JSON_Array const*  comps = json_object_get_array(goObj, "components");
            if (!comps) continue;

            const size_t compCount = json_array_get_count(comps);
            for (size_t j = 0; j < compCount; ++j)
            {
                JSON_Object const* compObj = json_array_get_object(comps, j);

                if (const char* type = json_object_get_string(compObj, "type");
                    !type || std::strcmp(type, "CMesh") != 0)
                    continue;

                const char* assetPath = json_object_get_string(compObj, "assetPath");
                if (!assetPath || assetPath[0] == '\0') continue;

                const char*       libRaw  = json_object_get_string(compObj, "libraryPath");
                const JSON_Value* uidVal  = json_object_get_value(compObj, "resourceUID");
                const JSON_Value* subIdxV = json_object_get_value(compObj, "submeshIndex");

                MeshRequest req;
                req.assetPath    = assetPath;
                req.libraryPath  = libRaw  ? libRaw : "";
                req.uid          = uidVal  ? static_cast<uint32>(json_value_get_number(uidVal))   : 0;
                req.submeshIndex = subIdxV ? static_cast<int32_t>(json_value_get_number(subIdxV)) : -1;

                outRequests[{req.assetPath, req.submeshIndex}] = std::move(req);
            }
        }
    }
} // namespace

ScenePreloader::ScenePreloader(IResourceLoader* resourceLoader)
    : m_resourceLoader(resourceLoader)
{
}

std::vector<std::future<void>> ScenePreloader::PreloadSceneResourcesAsync(
    NOUS_Multithreading::NOUS_JobSystem* jobSystem,
    const std::string& sceneFilePath)
{
    std::vector<std::future<void>> futures;

    JSON_Value* root = json_parse_file(sceneFilePath.c_str());
    if (!root)
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "PreloadSceneResourcesAsync: failed to parse '%s'", sceneFilePath.c_str());
        return futures;
    }

    JSON_Object const* rootObj     = json_value_get_object(root);
    JSON_Array const*  gameObjects = json_object_get_array(rootObj, "GameObjects");

    if (!gameObjects)
    {
        json_value_free(root);
        return futures;
    }

    std::map<std::pair<std::string, int32_t>, MeshRequest> uniqueRequests;
    CollectMeshRequestsFromScene(gameObjects, uniqueRequests);
    json_value_free(root);

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
