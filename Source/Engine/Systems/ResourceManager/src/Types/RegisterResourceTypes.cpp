#include <ResourceManager/Core/TypeRegistry.h>
#include <EngineCore/Casts.h>

#include <MemoryManager/MemoryManager.h>
#include <ResourceManager/Core/IImporter.h>

#include <ranges>

// Importers
#include <ResourceManager/Types/ResourceMesh/ImporterMesh.h>
#include <ResourceManager/Types/ResourceMaterial/ImporterMaterial.h>
#include "Types/ResourceTexture/ImporterTexture.h"
#include "Types/ResourceShader/ImporterShader.h"
#include "Types/ResourceAudio/ImporterAudio.h"
#include "Types/ResourceVideo/ImporterVideo.h"
#include "Types/ResourceScene/ImporterScene.h"
#include <ResourceManager/Types/ResourceAudioGraph/ImporterAudioGraph.h>

// Resources
#include <ResourceManager/Types/ResourceMesh/ResourceMesh.h>
#include <ResourceManager/Types/ResourceMaterial/ResourceMaterial.h>
#include <ResourceManager/Types/ResourceTexture/ResourceTexture.h>
#include <ResourceManager/Types/ResourceShader/ResourceShader.h>
#include <ResourceManager/Types/ResourceAudio/ResourceAudio.h>
#include <ResourceManager/Types/ResourceVideo/ResourceVideo.h>
#include <ResourceManager/Types/ResourceAudioGraph/ResourceAudioGraph.h>

namespace
{
    // Cleanup priorities — lower destroyed first.
    // Shaders go first (materials reference them), then materials (reference
    // textures + shaders), then leaf resources.
    constexpr int k_PrioShader   = 0;
    constexpr int k_PrioMaterial = 1;
    constexpr int k_PrioTexture  = 2;
    constexpr int k_PrioMesh     = 3;
    constexpr int k_PrioAudio    = 4;
    constexpr int k_PrioVideo    = 5;
    // Scenes own no runtime resource object and no peer-resource pointers, so
    // their cleanup priority is inert; kept last for readability.
    constexpr int k_PrioScene    = 6;
    // AudioGraph owns no peer-resource pointers (effects are self-contained), so
    // its cleanup priority is inert; ordered after scene for readability.
    constexpr int k_PrioAudioGraph = 7;
}

void RegisterResourceTypes(TypeRegistry& registry)
{
    // ---------- SHADER ----------
    {
        TypeDescriptor d;
        d.type = ResourceType::SHADER;
        d.name = "Shader";
        d.libraryFolder = "Library/Shaders/";
        d.libraryFixedExtension.clear();
        d.sourceExtensions = { "glsl", "spv" };
        d.libExtPolicy = LibraryExtPolicy::DIRECTORY_OF_STAGES;
        d.memoryTag = MemoryTag::RESOURCE_SHADER;
        d.cleanupPriority = k_PrioShader;
        d.SetImporter<ImporterShader>();
        d.createFn = [](uint32_t uid) -> ResourceBase* { return NOUS_NEW<ResourceShader>(MemoryTag::RESOURCE_SHADER, uid); };
        d.destroyFn = [](ResourceBase* r) { NOUS_DELETE(r, MemoryTag::RESOURCE_SHADER); };
        d.display.color[0] = 0.7f; d.display.color[1] = 0.2f; d.display.color[2] = 1.0f; d.display.color[3] = 1.0f;
        registry.Register(std::move(d));
    }

    // ---------- MATERIAL ----------
    {
        TypeDescriptor d;
        d.type = ResourceType::MATERIAL;
        d.name = "Material";
        d.libraryFolder = "Library/Materials/";
        d.libraryFixedExtension = "nmat";
        d.sourceExtensions = { "nmat" };
        d.libExtPolicy = LibraryExtPolicy::FIXED;
        d.memoryTag = MemoryTag::RESOURCE_MATERIAL;
        d.cleanupPriority = k_PrioMaterial;
        d.SetImporter<ImporterMaterial>();
        d.createFn = [](uint32_t uid) -> ResourceBase* { return NOUS_NEW<ResourceMaterial>(MemoryTag::RESOURCE_MATERIAL, uid); };
        d.destroyFn = [](ResourceBase* r) { NOUS_DELETE(r, MemoryTag::RESOURCE_MATERIAL); };
        d.hotReloadable = true;
        // Texture refs are still alive at this point (lower priority); null them
        // inline rather than routing through Evict, which would recurse via
        // UnloadResource into the half-torn-down resource manager.
        d.evictAtShutdown = [](ResourceBase* r)
        {
            for (auto& map : down_cast<ResourceMaterial*>(r)->textureMaps | std::views::values)
                map.texture = nullptr;
        };
        d.display.color[0] = 0.8f; d.display.color[1] = 0.5f; d.display.color[2] = 0.0f; d.display.color[3] = 1.0f;
        registry.Register(std::move(d));
    }

    // ---------- TEXTURE ----------
    {
        TypeDescriptor d;
        d.type = ResourceType::TEXTURE;
        d.name = "Texture";
        d.libraryFolder = "Library/Textures/";
        d.libraryFixedExtension = "png";
        d.sourceExtensions = { "png", "jpg", "jpeg", "tga" };
        d.libExtPolicy = LibraryExtPolicy::FIXED;
        d.memoryTag = MemoryTag::RESOURCE_TEXTURE;
        d.cleanupPriority = k_PrioTexture;
        d.SetImporter<ImporterTexture>();
        d.createFn = [](uint32_t uid) -> ResourceBase* { return NOUS_NEW<ResourceTexture>(MemoryTag::RESOURCE_TEXTURE, uid); };
        d.destroyFn = [](ResourceBase* r) { NOUS_DELETE(r, MemoryTag::RESOURCE_TEXTURE); };
        d.hotReloadable = true;
        d.display.color[0] = 0.5f; d.display.color[1] = 0.8f; d.display.color[2] = 0.0f; d.display.color[3] = 1.0f;
        registry.Register(std::move(d));
    }

    // ---------- MESH ----------
    {
        TypeDescriptor d;
        d.type = ResourceType::MESH;
        d.name = "Mesh";
        d.libraryFolder = "Library/Meshes/";
        d.libraryFixedExtension = "nmesh";
        d.sourceExtensions = { "fbx", "obj", "glb", "gltf", "nmesh" };
        d.libExtPolicy = LibraryExtPolicy::FIXED;
        d.memoryTag = MemoryTag::RESOURCE_MESH;
        d.cleanupPriority = k_PrioMesh;
        d.SetImporter<ImporterMesh>();
        d.createFn = [](uint32_t uid) -> ResourceBase* { return NOUS_NEW<ResourceMesh>(MemoryTag::RESOURCE_MESH, uid); };
        d.destroyFn = [](ResourceBase* r) { NOUS_DELETE(r, MemoryTag::RESOURCE_MESH); };
        d.display.color[0] = 0.0f; d.display.color[1] = 0.8f; d.display.color[2] = 0.5f; d.display.color[3] = 1.0f;
        registry.Register(std::move(d));
    }

    // ---------- AUDIO ----------
    {
        TypeDescriptor d;
        d.type = ResourceType::AUDIO;
        d.name = "Audio";
        d.libraryFolder = "Library/Audio/";
        d.libraryFixedExtension.clear();
        d.sourceExtensions = { "wav", "ogg" };
        d.libExtPolicy = LibraryExtPolicy::PRESERVE_SOURCE;
        d.memoryTag = MemoryTag::RESOURCE_AUDIO;
        d.cleanupPriority = k_PrioAudio;
        d.SetImporter<ImporterAudio>();
        d.createFn = [](uint32_t uid) -> ResourceBase* { return NOUS_NEW<ResourceAudio>(MemoryTag::RESOURCE_AUDIO, uid); };
        d.destroyFn = [](ResourceBase* r) { NOUS_DELETE(r, MemoryTag::RESOURCE_AUDIO); };
        d.display.color[0] = 0.93f; d.display.color[1] = 0.28f; d.display.color[2] = 0.60f; d.display.color[3] = 1.0f;
        registry.Register(std::move(d));
    }

    // ---------- VIDEO ----------
    {
        TypeDescriptor d;
        d.type = ResourceType::VIDEO;
        d.name = "Video";
        d.libraryFolder = "Library/Video/";
        d.libraryFixedExtension.clear();
        d.sourceExtensions = { "mp4", "gif" };
        d.libExtPolicy = LibraryExtPolicy::PRESERVE_SOURCE;
        d.memoryTag = MemoryTag::RESOURCE_VIDEO;
        d.cleanupPriority = k_PrioVideo;
        d.SetImporter<ImporterVideo>();
        d.createFn = [](uint32_t uid) -> ResourceBase* { return NOUS_NEW<ResourceVideo>(MemoryTag::RESOURCE_VIDEO, uid); };
        d.destroyFn = [](ResourceBase* r) { NOUS_DELETE(r, MemoryTag::RESOURCE_VIDEO); };
        d.display.color[0] = 0.60f; d.display.color[1] = 0.30f; d.display.color[2] = 0.93f; d.display.color[3] = 1.0f;
        registry.Register(std::move(d));
    }

    // ---------- SCENE ----------
    {
        TypeDescriptor d;
        d.type = ResourceType::SCENE;
        d.name = "Scene";
        d.libraryFolder = "Library/Scenes/";
        d.libraryFixedExtension = "nous";
        d.sourceExtensions = { "nous" };
        d.libExtPolicy = LibraryExtPolicy::FIXED;
        d.memoryTag = MemoryTag::SCENE;
        d.cleanupPriority = k_PrioScene;
        d.SetImporter<ImporterScene>();
        // No createFn/destroyFn: scenes have no runtime resource object. The
        // import step only mirrors the source .nous into Library/Scenes/ under
        // its UID; scene loading goes through ModuleScene, not the generic
        // Resource lifecycle. Null guards in the resource manager already cover
        // the absent create/destroy callbacks.
        d.display.color[0] = 0.25f; d.display.color[1] = 0.55f; d.display.color[2] = 0.95f; d.display.color[3] = 1.0f;
        registry.Register(std::move(d));
    }

    // ---------- AUDIO GRAPH ----------
    {
        TypeDescriptor d;
        d.type = ResourceType::AUDIO_GRAPH;
        d.name = "Audio Graph";
        d.libraryFolder = "Library/AudioGraphs/";
        d.libraryFixedExtension = "nafx";
        d.sourceExtensions = { "nafx" };
        d.libExtPolicy = LibraryExtPolicy::FIXED;
        d.memoryTag = MemoryTag::RESOURCE_AUDIO;   // reuse audio tag (MVP; dedicated tag is a later cleanup)
        d.cleanupPriority = k_PrioAudioGraph;
        d.SetImporter<ImporterAudioGraph>();
        d.createFn = [](uint32_t uid) -> ResourceBase* { return NOUS_NEW<ResourceAudioGraph>(MemoryTag::RESOURCE_AUDIO, uid); };
        d.destroyFn = [](ResourceBase* r) { NOUS_DELETE(r, MemoryTag::RESOURCE_AUDIO); };
        d.display.color[0] = 0.20f; d.display.color[1] = 0.85f; d.display.color[2] = 0.85f; d.display.color[3] = 1.0f;
        registry.Register(std::move(d));
    }
}
