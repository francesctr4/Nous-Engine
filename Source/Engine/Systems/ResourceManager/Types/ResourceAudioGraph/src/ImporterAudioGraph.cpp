#include "Engine/Systems/ResourceManager/Types/ResourceAudioGraph/include/ImporterAudioGraph.h"

#include "Engine/Systems/ResourceManager/Types/ResourceAudioGraph/include/ResourceAudioGraph.h"
#include <AudioSystem/AudioGraph/AudioEffectRegistry.h>

#include "Engine/Core/Globals.h"               // down_cast
#include "Engine/Core/FileSystem/FileSystem.h"
#include "Engine/Core/Logger/Logger.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/Utils/Serialization/JsonFile/JsonFile.h"
#include "Engine/Utils/Serialization/JsonFile/JsonObject.h"
#include "Engine/Utils/Serialization/JsonFile/JsonArray.h"
#include "Engine/Systems/ResourceManager/Core/ResourceBase/include/MetaFileData.h"

namespace ae = nous::audio;

bool ImporterAudioGraph::Import(const MetaFileData& metaFileData)
{
    // Reuse the audio memory tag (no dedicated AUDIO_GRAPH tag in the MVP).
    ResourceBase* temp = NOUS_NEW<ResourceAudioGraph>(MemoryTag::RESOURCE_AUDIO);
    return Save(metaFileData, temp);
}

bool ImporterAudioGraph::Save(const MetaFileData& metaFileData, ResourceBase*& inResource)
{
    NOUS_DELETE(inResource, MemoryTag::RESOURCE_AUDIO);

    // .nafx is authored JSON with no external asset references to enrich, so the
    // library copy is the source verbatim (unlike ImporterMaterial, which injects
    // texture/shader uids here).
    return nous::engine::filesystem::CopyFile(metaFileData.assetsPath, metaFileData.libraryPath);
}

bool ImporterAudioGraph::Deserialize(const std::string& libraryPath, ResourceBase* outResource)
{
    ResourceAudioGraph* graph = down_cast<ResourceAudioGraph*>(outResource);

    JsonObject root = JsonFile::LoadFromFile(libraryPath);
    if (root.IsEmpty())
    {
        NOUS_ERROR("ImporterAudioGraph::Deserialize() failed to load '%s'", libraryPath.c_str());
        return false;
    }

    graph->effects.clear();
    graph->editorPositions.clear();

    // ── effects[] ──
    JsonArray effectsArr = root.GetArray("effects");
    const int effectCount = effectsArr.Count();
    for (int i = 0; i < effectCount; ++i)
    {
        JsonObject entry = effectsArr.GetObject(i);
        if (entry.IsEmpty()) continue;

        const std::string typeStr = entry.GetString("type");
        AudioEffectType   type{};
        if (!ae::TypeFromString(typeStr, type))
        {
            NOUS_WARN("ImporterAudioGraph::Deserialize() unknown effect type '%s'; skipping.",
                      typeStr.c_str());
            continue;
        }

        AudioEffectDesc desc;
        desc.type = type;

        // Params are stored by name; re-pack into the schema's order, defaulting
        // any missing param. (Forward-compatible if a param is added later.)
        JsonObject paramsObj = entry.GetObject("params");
        for (const auto& p : ae::Params(type))
            desc.params.push_back(paramsObj.GetFloat(p.name, p.def));

        graph->effects.push_back(std::move(desc));
    }

    // ── editor block (opaque view-state) ──
    JsonObject editor = root.GetObject("editor");
    JsonArray  nodes  = editor.GetArray("nodes");
    const int  nodeCount = nodes.Count();
    for (int i = 0; i < nodeCount; ++i)
    {
        JsonObject n = nodes.GetObject(i);
        graph->editorPositions.push_back(glm::vec2(n.GetFloat("x"), n.GetFloat("y")));
    }

    return true;
}

void ImporterAudioGraph::Evict(ResourceBase* /*inResource*/)
{
    // No peer-resource references held; nothing to release.
}

bool ImporterAudioGraph::Upload(ResourceBase* /*outResource*/, IGPUResourceFactory* /*gpu*/)
{
    // No GPU residency — the DSP chain is built at runtime by the audio backend.
    return true;
}

void ImporterAudioGraph::Release(ResourceBase* /*inResource*/, IGPUResourceFactory* /*gpu*/)
{
    // No GPU handles held; nothing to release.
}

bool ImporterAudioGraph::WriteAudioGraphToFile(const ResourceAudioGraph& graph,
                                               const std::string& path)
{
    JsonObject root;

    JsonArray effectsArr;
    for (const auto& e : graph.effects)
    {
        JsonObject entry;
        entry.Set("type", ae::TypeToString(e.type));

        JsonObject paramsObj;
        std::span<const AudioEffectParamDesc> schema = ae::Params(e.type);
        for (size_t i = 0; i < schema.size() && i < e.params.size(); ++i)
            paramsObj.Set(schema[i].name, e.params[i]);
        entry.Set("params", std::move(paramsObj));

        effectsArr.Append(std::move(entry));
    }
    root.Set("effects", std::move(effectsArr));

    JsonObject editor;
    JsonArray  nodes;
    for (const glm::vec2& pos : graph.editorPositions)
    {
        JsonObject n;
        n.Set("x", pos.x);
        n.Set("y", pos.y);
        nodes.Append(std::move(n));
    }
    editor.Set("nodes", std::move(nodes));
    root.Set("editor", std::move(editor));

    return JsonFile::SaveToFile(root, path);
}

bool ImporterAudioGraph::CreateNewAudioGraphFile(const std::string& assetPath)
{
    if (assetPath.empty()) return false;

    nous::engine::filesystem::CreateDirectory(nous::engine::filesystem::GetDirectory(assetPath));

    JsonObject root;
    root.Set("effects", JsonArray{});

    JsonObject editor;
    editor.Set("nodes", JsonArray{});
    root.Set("editor", std::move(editor));

    return JsonFile::SaveToFile(root, assetPath);
}
